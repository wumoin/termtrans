/**
 * @file application.cpp
 * @brief 实现应用命令分发器。
 */

#include "app/application.h"

#include "cli/args.h"
#include "config/config.h"
#include "config/platform_paths.h"
#include "history/history_key.h"
#include "history/history_policy.h"
#include "history/history_record.h"
#include "history/sqlite_history_store.h"
#include "i18n/messages.h"
#include "input/reader.h"
#include "output/plain_writer.h"
#include "output/progress_indicator.h"
#include "prompt/prompt_registry.h"
#include "translate/curl_http_client.h"
#include "translate/openai_compatible_translator.h"
#include "translate/translation_session.h"

#include <charconv>
#include <cstdio>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <istream>
#include <ostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>

#ifdef _WIN32
#include <io.h>
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

namespace termtrans::app {
namespace {

/**
 * @brief 判断当前 stdin 是否连接到交互式终端。
 *
 * 当前只用它判断无位置参数调用时是否应提示缺少输入。后续输入模块会在
 * 真实翻译流程中接管 stdin 读取。
 */
bool IsStdinInteractive() {
#ifdef _WIN32
  return _isatty(_fileno(stdin)) != 0;
#else
  return isatty(STDIN_FILENO) != 0;
#endif
}

/**
 * @brief 判断 stderr 是否连接到交互式终端。
 *
 * 该结果只用于非流式进度提示和历史命中确认。stdout 是否为终端不影响
 * 译文输出策略，避免破坏 Unix filter 行为。
 */
bool IsStderrInteractive() {
#ifdef _WIN32
  return _isatty(_fileno(stderr)) != 0;
#else
  return isatty(STDERR_FILENO) != 0;
#endif
}

/**
 * @brief 判断 stdout 是否连接到交互式终端。
 *
 * 历史命中确认会等待用户输入；当 stdout 已接入管道或重定向时，尤其是
 * `termtrans | less`，下游程序可能也在控制终端上等待输入。此时不主动
 * 询问，避免提示和下游阅读界面抢占终端。
 */
bool IsStdoutInteractive() {
#ifdef _WIN32
  return _isatty(_fileno(stdout)) != 0;
#else
  return isatty(STDOUT_FILENO) != 0;
#endif
}

/**
 * @brief 判断交互提示是否可以写给用户。
 *
 * 真实 CLI 运行时只把真实 stderr 视为交互提示通道；测试通过注入
 * 非 std::cerr 的流来捕获提示，因此允许测试流承担交互输出。
 *
 * @param stderr_stream 诊断和提示输出流。
 * @return 可以输出交互提示时返回 true。
 */
bool CanWritePrompt(const std::ostream& stderr_stream) {
  if (&stderr_stream != &std::cerr) {
    return true;
  }

  return IsStderrInteractive();
}

/**
 * @brief 打开当前进程的控制终端作为交互输入。
 *
 * 管道输入时 stdin 承载待翻译文本，历史命中确认不能继续从 stdin 读，
 * 否则会在源文本读完后直接遇到 EOF。控制终端不可用时返回 false，
 * 调用方应退化为不可交互行为。
 *
 * @param terminal_input 输出参数；成功时持有已打开的控制终端输入流。
 * @return 成功打开控制终端时返回 true。
 */
bool OpenControlTerminalInput(std::ifstream* terminal_input) {
#ifdef _WIN32
  terminal_input->open("CONIN$");
#else
  terminal_input->open("/dev/tty");
#endif
  return terminal_input->is_open() && terminal_input->good();
}

/**
 * @brief 选择历史确认问题使用的输入流。
 *
 * 测试注入的 stdin_stream 保持原样，便于单元测试控制回答。真实运行时：
 * stdin 是终端则直接读取 stdin；stdin 是管道或重定向文件时，改读控制
 * 终端，避免把待翻译文本流和确认回答混在一起。
 *
 * @param stdin_stream 翻译输入流。
 * @param terminal_input 控制终端输入流持有者，不能为空。
 * @return 可读取回答的输入流；没有可用交互输入时返回 nullptr。
 */
std::istream* ResolvePromptInputStream(std::istream& stdin_stream,
                                       std::ifstream* terminal_input) {
  if (&stdin_stream != &std::cin || IsStdinInteractive()) {
    return &stdin_stream;
  }

  if (OpenControlTerminalInput(terminal_input)) {
    return terminal_input;
  }

  return nullptr;
}

/**
 * @brief 将 CLI 解析错误映射为本地化文案标识。
 *
 * 解析层只暴露错误类别，不直接依赖 i18n 文案；应用层在这里完成
 * 两个模块之间的边界转换。
 *
 * @param error_kind CLI 解析错误类别。
 * @return 对应的用户可见文案标识。
 */
i18n::MessageId MessageForParseError(cli::ParseErrorKind error_kind) {
  switch (error_kind) {
    case cli::ParseErrorKind::kMissingValue:
      return i18n::MessageId::kMissingOptionValue;
    case cli::ParseErrorKind::kInvalidUsage:
      return i18n::MessageId::kInvalidUsage;
    case cli::ParseErrorKind::kNone:
    case cli::ParseErrorKind::kUnknownArgument:
      return i18n::MessageId::kUnknownArgument;
  }

  return i18n::MessageId::kUnknownArgument;
}

/**
 * @brief 将 CLI 覆盖项转换为配置模块可合并的覆盖项。
 *
 * 该转换不验证值是否合法；配置模块会在 `ResolveConfig()` 中按当前支持
 * 范围处理非法值。
 *
 * @param cli_overrides 参数解析得到的覆盖项。
 * @return 配置合并使用的覆盖项。
 */
config::ConfigOverrides ToConfigOverrides(
    const cli::CliOverrides& cli_overrides) {
  return config::ConfigOverrides{
      .target_language = cli_overrides.target_language,
      .model_name = cli_overrides.model_name,
      .stream = cli_overrides.stream,
      .history_mode = cli_overrides.history_mode,
  };
}

/**
 * @brief 将输入读取错误映射为本地化文案标识。
 *
 * @param error_kind 输入读取错误类别。
 * @return 对应用户可见文案。
 */
i18n::MessageId MessageForReadInputError(
    input::ReadInputErrorKind error_kind) {
  switch (error_kind) {
    case input::ReadInputErrorKind::kInvalidUsage:
      return i18n::MessageId::kInvalidUsage;
    case input::ReadInputErrorKind::kReadFailed:
      return i18n::MessageId::kInputReadFailed;
    case input::ReadInputErrorKind::kNone:
    case input::ReadInputErrorKind::kEmptyInput:
      return i18n::MessageId::kEmptyInput;
  }

  return i18n::MessageId::kEmptyInput;
}

/**
 * @brief 判断用户确认输入是否为 yes。
 *
 * @param answer 用户输入文本。
 * @return y 或 yes 时返回 true，大小写不敏感。
 */
bool IsYes(std::string_view answer) {
  std::string lowered;
  lowered.reserve(answer.size());
  for (const char character : answer) {
    lowered.push_back(
        static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
  }

  return lowered == "y" || lowered == "yes";
}

/**
 * @brief 读取一行交互输入。
 *
 * @param prompt 已本地化的问题文本。
 * @param input_stream 交互输入流。
 * @param stderr_stream 交互提示输出流。
 * @return 用户输入；读取失败时返回空字符串。
 */
std::string PromptLine(std::string_view prompt,
                       std::istream& input_stream,
                       std::ostream& stderr_stream) {
  stderr_stream << prompt;
  stderr_stream.flush();

  std::string value;
  std::getline(input_stream, value);
  return value;
}

/**
 * @brief 在真实终端中尽量关闭回显读取 API key。
 *
 * 测试或非交互输入流无法关闭回显，此时退化为普通 getline。API key 不会
 * 写入 stdout、stderr、测试或文档。
 *
 * @param prompt 已本地化的问题文本。
 * @param input_stream 输入流。
 * @param stderr_stream 交互提示输出流。
 * @return 用户输入的 API key。
 */
std::string PromptSecretLine(std::string_view prompt,
                             std::istream& input_stream,
                             std::ostream& stderr_stream) {
  stderr_stream << prompt;
  stderr_stream.flush();

  if (&input_stream != &std::cin || !IsStdinInteractive()) {
    std::string value;
    std::getline(input_stream, value);
    return value;
  }

#ifdef _WIN32
  std::string value;
  for (;;) {
    const int character = _getch();
    if (character == '\r' || character == '\n') {
      stderr_stream << '\n';
      break;
    }
    if (character == '\b') {
      if (!value.empty()) {
        value.pop_back();
      }
      continue;
    }
    value.push_back(static_cast<char>(character));
  }
  return value;
#else
  termios old_terminal{};
  if (tcgetattr(STDIN_FILENO, &old_terminal) != 0) {
    std::string value;
    std::getline(input_stream, value);
    return value;
  }

  termios no_echo = old_terminal;
  no_echo.c_lflag &= ~ECHO;
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &no_echo);
  std::string value;
  std::getline(input_stream, value);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_terminal);
  stderr_stream << '\n';
  return value;
#endif
}

/**
 * @brief 解析交互式模型添加流程中的请求超时秒数。
 *
 * 留空表示沿用 `ModelProfile` 的默认值 60 秒；0 是合法值，表示不设置
 * provider HTTP 请求的整体超时上限。其它非负整数按秒数保存。
 *
 * @param value 用户输入的原始文本。
 * @return 合法时返回超时秒数；负数、非数字或溢出时返回空。
 */
std::optional<int> ParseTimeoutSecondsInput(std::string_view value) {
  if (value.empty()) {
    return config::ModelProfile{}.timeout_seconds;
  }

  int timeout_seconds = 0;
  const char* begin = value.data();
  const char* end = begin + value.size();
  const std::from_chars_result result =
      std::from_chars(begin, end, timeout_seconds);
  if (result.ec != std::errc() || result.ptr != end ||
      timeout_seconds < 0) {
    return std::nullopt;
  }

  return timeout_seconds;
}

/**
 * @brief 输出当前模型配置列表。
 *
 * @param config 已读取的应用配置。
 * @param resolved_config 已合并的运行配置。
 * @param ui_language 已解析 UI 语言。
 * @param stdout_stream 成功输出流。
 */
void WriteModelsList(const config::AppConfig& config,
                     const config::ResolvedConfig& resolved_config,
                     std::string_view ui_language,
                     std::ostream& stdout_stream) {
  if (config.models.empty()) {
    stdout_stream << i18n::Messages::Get(i18n::MessageId::kModelsListEmpty,
                                         {.language = ui_language})
                  << '\n';
    return;
  }

  stdout_stream << i18n::Messages::Get(i18n::MessageId::kModelsListHeader,
                                       {.language = ui_language})
                << '\n';
  for (const config::ModelProfile& profile : config.models) {
    stdout_stream << profile.name << '\t' << profile.provider_type << '\t'
                  << profile.model_id << '\t'
                  << (profile.name == resolved_config.default_model ? "yes"
                                                                    : "no")
                  << '\n';
  }
}

/**
 * @brief 执行默认模型设置命令。
 */
int SetDefaultModelCommand(const cli::ParseResult& parse_result,
                           const std::filesystem::path& config_path,
                           std::string_view ui_language,
                           std::ostream& stdout_stream,
                           std::ostream& stderr_stream) {
  const config::SaveConfigResult save_result =
      config::SetDefaultModel(config_path, parse_result.model_name);
  if (!save_result.is_ok &&
      (save_result.error_message == "model not found" ||
       save_result.error_message == "invalid model name")) {
    stderr_stream << i18n::Messages::Get(
                         i18n::MessageId::kModelNotFound,
                         {.argument = parse_result.model_name,
                          .language = ui_language})
                  << '\n';
    return 2;
  }

  if (!save_result.is_ok) {
    stderr_stream << i18n::Messages::Get(
                         i18n::MessageId::kConfigWriteFailed,
                         {.path = config_path.string(), .language = ui_language})
                  << '\n';
    return 1;
  }

  stdout_stream << i18n::Messages::Get(
                       i18n::MessageId::kModelsSetDefaultSuccess,
                       {.argument = parse_result.model_name,
                        .path = config_path.string(),
                        .language = ui_language})
                << '\n';
  return 0;
}

/**
 * @brief 使用 provider 执行添加模型前的最小测试调用。
 */
translate::TranslateResult TestModelProfile(
    const config::ModelProfile& profile) {
  translate::CurlHttpClient http_client;
  translate::OpenAICompatibleTranslator translator(profile, &http_client);
  return translator.Translate(
      translate::TranslateRequest{
          .target_language = "zh-CN",
          .system_prompt =
              "把输入文本翻译成简体中文。只输出译文，不要解释。",
          .source_text = "hello world",
          .stream = false,
      },
      nullptr);
}

/**
 * @brief 执行交互式添加模型命令。
 */
int AddModelCommand(const std::filesystem::path& config_path,
                    std::string_view ui_language,
                    std::istream& stdin_stream,
                    std::ostream& stdout_stream,
                    std::ostream& stderr_stream) {
  config::ModelProfile profile;
  profile.name = PromptLine(
      i18n::Messages::Get(i18n::MessageId::kModelAddPromptName,
                          {.language = ui_language}),
      stdin_stream, stderr_stream);
  profile.provider_type = PromptLine(
      i18n::Messages::Get(i18n::MessageId::kModelAddPromptProviderType,
                          {.language = ui_language}),
      stdin_stream, stderr_stream);
  if (profile.provider_type.empty()) {
    profile.provider_type = "openai-compatible";
  }
  profile.base_url = PromptLine(
      i18n::Messages::Get(i18n::MessageId::kModelAddPromptBaseUrl,
                          {.language = ui_language}),
      stdin_stream, stderr_stream);
  profile.model_id = PromptLine(
      i18n::Messages::Get(i18n::MessageId::kModelAddPromptModelId,
                          {.language = ui_language}),
      stdin_stream, stderr_stream);
  profile.api_key = PromptSecretLine(
      i18n::Messages::Get(i18n::MessageId::kModelAddPromptApiKey,
                          {.language = ui_language}),
      stdin_stream, stderr_stream);
  const std::string timeout_seconds_text = PromptLine(
      i18n::Messages::Get(i18n::MessageId::kModelAddPromptTimeoutSeconds,
                          {.language = ui_language}),
      stdin_stream, stderr_stream);
  const std::optional<int> timeout_seconds =
      ParseTimeoutSecondsInput(timeout_seconds_text);
  if (!timeout_seconds.has_value()) {
    stderr_stream << i18n::Messages::Get(
                         i18n::MessageId::kModelInvalidValue,
                         {.argument = "timeout_seconds",
                          .language = ui_language})
                  << '\n';
    return 2;
  }
  profile.timeout_seconds = *timeout_seconds;
  const std::string set_default_answer = PromptLine(
      i18n::Messages::Get(i18n::MessageId::kModelAddPromptSetDefault,
                          {.language = ui_language}),
      stdin_stream, stderr_stream);
  const bool set_default = IsYes(set_default_answer);

  if (!config::IsValidModelProfile(profile)) {
    stderr_stream << i18n::Messages::Get(
                         i18n::MessageId::kModelInvalidValue,
                         {.argument = profile.name, .language = ui_language})
                  << '\n';
    return 2;
  }

  stderr_stream << i18n::Messages::Get(i18n::MessageId::kModelAddTesting,
                                       {.language = ui_language})
                << '\n';
  const translate::TranslateResult test_result = TestModelProfile(profile);
  if (!test_result.is_ok) {
    stderr_stream << i18n::Messages::Get(
                         i18n::MessageId::kModelAddFailed,
                         {.argument = test_result.error_message,
                          .language = ui_language})
                  << '\n';
    return 1;
  }

  const config::SaveConfigResult save_result =
      config::SaveModelProfile(config_path, profile, set_default);
  if (!save_result.is_ok) {
    stderr_stream << i18n::Messages::Get(
                         i18n::MessageId::kConfigWriteFailed,
                         {.path = config_path.string(), .language = ui_language})
                  << '\n';
    return 1;
  }

  stdout_stream << i18n::Messages::Get(
                       i18n::MessageId::kModelAddSuccess,
                       {.argument = profile.name,
                        .path = config_path.string(),
                        .language = ui_language})
                << '\n';
  return 0;
}

/**
 * @brief 执行 Plain 翻译主流程。
 */
int TranslateCommand(const cli::ParseResult& parse_result,
                     const config::AppConfig& config,
                     const config::ResolvedConfig& resolved_config,
                     const prompt::PromptRegistry& prompt_registry,
                     const std::filesystem::path& history_path,
                     std::istream& stdin_stream,
                     std::ostream& stdout_stream,
                     std::ostream& stderr_stream) {
  const std::optional<prompt::PromptEntry> prompt_entry =
      prompt_registry.Resolve(resolved_config.target_language);
  if (!prompt_entry.has_value()) {
    stderr_stream << i18n::Messages::Get(
                         i18n::MessageId::kLanguageNotFound,
                         {.argument = resolved_config.target_language,
                          .language = resolved_config.ui_language})
                  << '\n';
    return 2;
  }

  if (resolved_config.default_model.empty()) {
    stderr_stream << i18n::Messages::Get(
                         i18n::MessageId::kDefaultModelMissing,
                         {.language = resolved_config.ui_language})
                  << '\n';
    return 1;
  }

  const config::ModelProfile* model_profile =
      config::FindModelProfile(config, resolved_config.default_model);
  if (model_profile == nullptr) {
    stderr_stream << i18n::Messages::Get(
                         i18n::MessageId::kModelNotFound,
                         {.argument = resolved_config.default_model,
                          .language = resolved_config.ui_language})
                  << '\n';
    return 2;
  }

  const input::ReadInputResult input_result = input::ReadInput(
      parse_result.positionals, stdin_stream,
      input::ReadInputOptions{
          .is_stdin_interactive = IsStdinInteractive(),
      });
  if (!input_result.is_ok) {
    const i18n::MessageId message_id =
        MessageForReadInputError(input_result.error_kind);
    stderr_stream << i18n::Messages::Get(
                         message_id,
                         {.argument = "translate",
                          .language = resolved_config.ui_language})
                  << '\n';
    return input_result.error_kind == input::ReadInputErrorKind::kInvalidUsage
               ? 2
               : 1;
  }

  history::SqliteHistoryStore history_store(history_path);
  const history::HistoryKey history_key = history::ComputeHistoryKey(
      input_result.text, resolved_config.target_language);
  if (history::ShouldLookupHistory(resolved_config.history_mode)) {
    const history::HistoryLookupResult lookup_result =
        history_store.Lookup(history_key);
    if (!lookup_result.is_ok) {
      stderr_stream << i18n::Messages::Get(
                           i18n::MessageId::kHistoryReadFailed,
                           {.argument = lookup_result.error_message,
                            .language = resolved_config.ui_language})
                    << '\n';
      return 1;
    }

    if (lookup_result.has_record) {
      std::ifstream terminal_prompt_input;
      std::istream* prompt_input_stream = nullptr;
      const bool can_ask = CanWritePrompt(stderr_stream) && IsStdoutInteractive();
      if (can_ask) {
        prompt_input_stream =
            ResolvePromptInputStream(stdin_stream, &terminal_prompt_input);
      }

      const history::HistoryHitAction action =
          history::DecideHistoryHitAction(resolved_config.history_mode,
                                          prompt_input_stream != nullptr &&
                                              can_ask);
      bool should_retranslate = action == history::HistoryHitAction::kTranslate;
      if (action == history::HistoryHitAction::kAsk) {
        const std::string answer = PromptLine(
            i18n::Messages::Get(i18n::MessageId::kHistoryHitPrompt,
                                {.language = resolved_config.ui_language}),
            *prompt_input_stream, stderr_stream);
        should_retranslate = IsYes(answer);
      }

      if (!should_retranslate) {
        output::PlainWriter history_writer(stdout_stream);
        if (!history_writer.Write(lookup_result.record.translated_text)) {
          stderr_stream << i18n::Messages::Get(
                               i18n::MessageId::kOutputCancelled,
                               {.language = resolved_config.ui_language})
                        << '\n';
        }
        return 0;
      }
    }
  }

  translate::CurlHttpClient http_client;
  translate::OpenAICompatibleTranslator translator(*model_profile,
                                                   &http_client);
  translate::TranslationSession session(
      &translator,
      translate::Chunker(static_cast<std::size_t>(
          resolved_config.max_input_bytes)));
  output::PlainWriter writer(stdout_stream);
  output::ProgressIndicator progress(
      stderr_stream, !resolved_config.stream && IsStderrInteractive());
  progress.Start(i18n::Messages::Get(i18n::MessageId::kTranslationProgress,
                                     {.language = resolved_config.ui_language}));
  const translate::TranslationSessionResult result = session.Run(
      translate::TranslationSessionRequest{
          .target_language = resolved_config.target_language,
          .system_prompt = prompt_entry->prompt,
          .input_text = input_result.text,
          .stream = resolved_config.stream,
      },
      &writer);
  progress.Finish();

  if (!result.is_ok) {
    const i18n::MessageId message_id =
        result.is_cancelled ? i18n::MessageId::kOutputCancelled
                            : i18n::MessageId::kTranslationFailed;
    stderr_stream << i18n::Messages::Get(
                         message_id,
                         {.argument = result.error_message,
                          .language = resolved_config.ui_language})
                  << '\n';
    return result.is_cancelled ? 0 : 1;
  }

  if (history::ShouldSaveHistory(resolved_config.history_mode)) {
    const history::HistoryRecord record{
        .key = history_key,
        .input_text = input_result.text,
        .translated_text = result.translated_text,
        .model_name = model_profile->name,
        .provider_type = model_profile->provider_type,
        .model_id = model_profile->model_id,
        .base_url = model_profile->base_url,
        .input_bytes = static_cast<std::uint64_t>(input_result.text.size()),
        .output_bytes =
            static_cast<std::uint64_t>(result.translated_text.size()),
    };
    const history::HistoryMutationResult save_result =
        history_store.Save(record);
    if (!save_result.is_ok) {
      stderr_stream << i18n::Messages::Get(
                           i18n::MessageId::kHistoryWriteFailed,
                           {.argument = save_result.error_message,
                            .language = resolved_config.ui_language})
                    << '\n';
      return 1;
    }
  }

  return 0;
}

/**
 * @brief 输出历史记录元数据列表。
 *
 * @param history_path 历史数据库路径。
 * @param ui_language 已解析 UI 语言。
 * @param stdout_stream 成功输出流。
 * @param stderr_stream 诊断输出流。
 * @return 进程退出码。
 */
int ListHistoryCommand(const std::filesystem::path& history_path,
                       std::string_view ui_language,
                       std::ostream& stdout_stream,
                       std::ostream& stderr_stream) {
  history::SqliteHistoryStore history_store(history_path);
  const history::HistoryListResult list_result = history_store.List();
  if (!list_result.is_ok) {
    stderr_stream << i18n::Messages::Get(
                         i18n::MessageId::kHistoryReadFailed,
                         {.argument = list_result.error_message,
                          .language = ui_language})
                  << '\n';
    return 1;
  }

  if (list_result.records.empty()) {
    stdout_stream << i18n::Messages::Get(i18n::MessageId::kHistoryListEmpty,
                                         {.language = ui_language})
                  << '\n';
    return 0;
  }

  stdout_stream << i18n::Messages::Get(i18n::MessageId::kHistoryListHeader,
                                       {.language = ui_language})
                << '\n';
  for (const history::HistoryRecord& record : list_result.records) {
    stdout_stream << record.updated_at << '\t' << record.key.target_language
                  << '\t' << record.input_bytes << '\t'
                  << record.output_bytes << '\t' << record.model_name << '\t'
                  << record.key.input_sha256 << '\n';
  }

  return 0;
}

/**
 * @brief 清空全部历史记录。
 *
 * @param history_path 历史数据库路径。
 * @param ui_language 已解析 UI 语言。
 * @param stdout_stream 成功输出流。
 * @param stderr_stream 诊断输出流。
 * @return 进程退出码。
 */
int ClearHistoryCommand(const std::filesystem::path& history_path,
                        std::string_view ui_language,
                        std::ostream& stdout_stream,
                        std::ostream& stderr_stream) {
  history::SqliteHistoryStore history_store(history_path);
  const history::HistoryMutationResult clear_result = history_store.Clear();
  if (!clear_result.is_ok) {
    stderr_stream << i18n::Messages::Get(
                         i18n::MessageId::kHistoryWriteFailed,
                         {.argument = clear_result.error_message,
                          .language = ui_language})
                  << '\n';
    return 1;
  }

  stdout_stream << i18n::Messages::Get(i18n::MessageId::kHistoryClearSuccess,
                                       {.language = ui_language})
                << '\n';
  return 0;
}

/**
 * @brief 读取用户提供的 prompt 文件。
 *
 * 应用层负责文件系统访问，避免 CLI 解析层和 PromptRegistry 依赖外部
 * 状态。读取结果保留原始换行和非 ASCII 内容。
 *
 * @param path prompt 文件路径。
 * @return 读取成功时返回完整文件内容；打开或读取失败时返回空。
 */
std::optional<std::string> ReadPromptFile(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input.is_open()) {
    return std::nullopt;
  }

  std::ostringstream buffer;
  buffer << input.rdbuf();
  if (!input.good() && !input.eof()) {
    return std::nullopt;
  }

  return buffer.str();
}

/**
 * @brief 输出当前可用目标语言和 prompt 来源。
 *
 * @param registry 当前配置下的 prompt 注册表。
 * @param ui_language 已解析的终端界面语言。
 * @param stdout_stream 成功输出流。
 */
void WriteLanguagesList(const prompt::PromptRegistry& registry,
                        std::string_view ui_language,
                        std::ostream& stdout_stream) {
  stdout_stream << i18n::Messages::Get(
                       i18n::MessageId::kLanguagesListHeader,
                       {.language = ui_language})
                << '\n';

  for (const prompt::PromptEntry& entry : registry.ListLanguages()) {
    stdout_stream << entry.code << '\t' << entry.display_name << '\t'
                  << prompt::PromptSourceName(entry.source) << '\n';
  }
}

/**
 * @brief 展示一个目标语言当前生效的完整 prompt。
 *
 * @param parse_result 已解析的 CLI 命令。
 * @param registry 当前配置下的 prompt 注册表。
 * @param ui_language 已解析的终端界面语言。
 * @param stdout_stream 成功输出流。
 * @param stderr_stream 诊断输出流。
 * @return 进程退出码。
 */
int ShowLanguagePrompt(const cli::ParseResult& parse_result,
                       const prompt::PromptRegistry& registry,
                       std::string_view ui_language,
                       std::ostream& stdout_stream,
                       std::ostream& stderr_stream) {
  const std::optional<prompt::PromptEntry> entry =
      registry.Resolve(parse_result.language);
  if (!entry.has_value()) {
    stderr_stream << i18n::Messages::Get(
                         i18n::MessageId::kLanguageNotFound,
                         {.argument = parse_result.language,
                          .language = ui_language})
                  << '\n';
    return 2;
  }

  stdout_stream << i18n::Messages::Get(
                       i18n::MessageId::kLanguagePromptHeader,
                       {.argument = entry->code,
                        .value = prompt::PromptSourceName(entry->source),
                        .language = ui_language})
                << entry->prompt << '\n';
  return 0;
}

/**
 * @brief 从用户文件写入一个目标语言的完整 prompt。
 *
 * @param parse_result 已解析的 CLI 命令。
 * @param config_path 默认配置文件路径。
 * @param ui_language 已解析的终端界面语言。
 * @param stdout_stream 成功输出流。
 * @param stderr_stream 诊断输出流。
 * @return 进程退出码。
 */
int SetLanguagePrompt(const cli::ParseResult& parse_result,
                      const std::filesystem::path& config_path,
                      std::string_view ui_language,
                      std::ostream& stdout_stream,
                      std::ostream& stderr_stream) {
  const std::optional<std::string> prompt_text =
      ReadPromptFile(parse_result.prompt_file);
  if (!prompt_text.has_value()) {
    stderr_stream << i18n::Messages::Get(
                         i18n::MessageId::kPromptFileReadFailed,
                         {.path = parse_result.prompt_file,
                          .language = ui_language})
                  << '\n';
    return 1;
  }

  const config::SaveConfigResult save_result =
      config::SavePrompt(config_path, parse_result.language, *prompt_text);
  if (!save_result.is_ok &&
      (save_result.error_message == "invalid language" ||
       save_result.error_message == "invalid prompt")) {
    stderr_stream << i18n::Messages::Get(
                         i18n::MessageId::kPromptInvalidValue,
                         {.argument = parse_result.language,
                          .path = parse_result.prompt_file,
                          .language = ui_language})
                  << '\n';
    return 2;
  }

  if (!save_result.is_ok) {
    stderr_stream << i18n::Messages::Get(
                         i18n::MessageId::kConfigWriteFailed,
                         {.path = config_path.string(), .language = ui_language})
                  << '\n';
    return 1;
  }

  stdout_stream << i18n::Messages::Get(
                       i18n::MessageId::kLanguageSetSuccess,
                       {.argument = parse_result.language,
                        .path = config_path.string(),
                        .language = ui_language})
                << '\n';
  return 0;
}

/**
 * @brief 删除一个目标语言的配置 prompt。
 *
 * 缺少对应配置项时仍返回成功，保持 `languages unset` 幂等；内置语言会
 * 因配置覆盖删除而恢复内置 prompt，自定义语言则从可用集合中消失。
 *
 * @param parse_result 已解析的 CLI 命令。
 * @param config_path 默认配置文件路径。
 * @param ui_language 已解析的终端界面语言。
 * @param stdout_stream 成功输出流。
 * @param stderr_stream 诊断输出流。
 * @return 进程退出码。
 */
int UnsetLanguagePrompt(const cli::ParseResult& parse_result,
                        const std::filesystem::path& config_path,
                        std::string_view ui_language,
                        std::ostream& stdout_stream,
                        std::ostream& stderr_stream) {
  const config::SaveConfigResult save_result =
      config::UnsetPrompt(config_path, parse_result.language);
  if (!save_result.is_ok && save_result.error_message == "invalid language") {
    stderr_stream << i18n::Messages::Get(
                         i18n::MessageId::kPromptInvalidValue,
                         {.argument = parse_result.language,
                          .language = ui_language})
                  << '\n';
    return 2;
  }

  if (!save_result.is_ok) {
    stderr_stream << i18n::Messages::Get(
                         i18n::MessageId::kConfigWriteFailed,
                         {.path = config_path.string(), .language = ui_language})
                  << '\n';
    return 1;
  }

  const i18n::MessageId message_id =
      save_result.did_change ? i18n::MessageId::kLanguageUnsetSuccess
                             : i18n::MessageId::kLanguageUnsetNoConfig;
  stdout_stream << i18n::Messages::Get(
                       message_id,
                       {.argument = parse_result.language,
                        .path = config_path.string(),
                        .language = ui_language})
                << '\n';
  return 0;
}

}  // 匿名命名空间

/**
 * @brief 执行一次 CLI 调用。
 *
 * 该函数负责把参数解析、配置读取、配置写入、目标语言 Prompt 管理、
 * 模型管理、历史命令和 Plain 翻译主流程串起来。
 *
 * @param argc main() 传入的参数数量。
 * @param argv main() 传入的参数数组。
 * @param stdout_stream 成功命令输出流。
 * @param stderr_stream 诊断、交互提示和进度输出流。
 * @return 进程退出码。
 */
int Application::Run(int argc,
                     char* argv[],
                     std::ostream& stdout_stream,
                     std::ostream& stderr_stream) const {
  return Run(argc, argv, std::cin, stdout_stream, stderr_stream);
}

/**
 * @brief 执行一次 CLI 调用，并允许测试注入 stdin。
 *
 * 该函数负责把参数解析、配置读取、配置写入、目标语言 Prompt 管理、模型
 * 管理、历史命令和 Plain 翻译主流程串起来。
 *
 * @param argc main() 传入的参数数量。
 * @param argv main() 传入的参数数组。
 * @param stdin_stream 翻译主流程读取输入的来源。
 * @param stdout_stream 成功命令和译文输出流。
 * @param stderr_stream 诊断、交互提示和进度输出流。
 * @return 进程退出码。
 */
int Application::Run(int argc,
                     char* argv[],
                     std::istream& stdin_stream,
                     std::ostream& stdout_stream,
                     std::ostream& stderr_stream) const {
  const cli::ParseResult parse_result = cli::ParseArgs(argc, argv);
  const std::filesystem::path config_path = config::DefaultConfigPath();
  const std::filesystem::path history_path = config::DefaultHistoryPath();
  const config::LoadConfigResult load_result = config::LoadConfig(config_path);
  const std::string ui_language =
      config::ResolveUiLanguage(load_result.config);

  if (parse_result.status == cli::ParseStatus::kError) {
    stderr_stream << i18n::Messages::Get(
                         MessageForParseError(parse_result.error_kind),
                         {.argument = parse_result.error_argument,
                          .language = ui_language})
                  << '\n';
    return 2;
  }

  if (!load_result.is_ok) {
    stderr_stream << i18n::Messages::Get(
                         i18n::MessageId::kConfigReadFailed,
                         {.path = config_path.string(), .language = ui_language})
                  << '\n';
    return 1;
  }

  if (parse_result.show_help) {
    stdout_stream << i18n::Messages::Get(i18n::MessageId::kHelpText,
                                         {.language = ui_language});
    return 0;
  }

  if (parse_result.command == cli::CommandKind::kConfigSet) {
    const config::SaveConfigResult save_result = config::SaveConfigValue(
        config_path, parse_result.config_key, parse_result.config_value);
    if (!save_result.is_ok &&
        (save_result.error_message == "unsupported config key" ||
         save_result.error_message == "invalid config value")) {
      stderr_stream << i18n::Messages::Get(
                           i18n::MessageId::kConfigInvalidValue,
                           {.key = parse_result.config_key,
                            .value = parse_result.config_value,
                            .language = ui_language})
                    << '\n';
      return 2;
    }

    if (!save_result.is_ok) {
      stderr_stream << i18n::Messages::Get(
                           i18n::MessageId::kConfigWriteFailed,
                           {.path = config_path.string(),
                            .language = ui_language})
                    << '\n';
      return 1;
    }

    stdout_stream << i18n::Messages::Get(
                         i18n::MessageId::kConfigSetSuccess,
                         {.key = parse_result.config_key,
                          .value = parse_result.config_value,
                          .path = config_path.string(),
                          .language = ui_language})
                  << '\n';
    return 0;
  }

  if (parse_result.command == cli::CommandKind::kUiLanguagesList) {
    stdout_stream << i18n::Messages::Get(i18n::MessageId::kUiLanguagesList,
                                         {.language = ui_language})
                  << '\n';
    return 0;
  }

  const prompt::PromptRegistry prompt_registry(load_result.config);

  if (parse_result.command == cli::CommandKind::kLanguagesList) {
    WriteLanguagesList(prompt_registry, ui_language, stdout_stream);
    return 0;
  }

  if (parse_result.command == cli::CommandKind::kLanguagesShow) {
    return ShowLanguagePrompt(parse_result, prompt_registry, ui_language,
                              stdout_stream, stderr_stream);
  }

  if (parse_result.command == cli::CommandKind::kLanguagesSet) {
    return SetLanguagePrompt(parse_result, config_path, ui_language,
                             stdout_stream, stderr_stream);
  }

  if (parse_result.command == cli::CommandKind::kLanguagesUnset) {
    return UnsetLanguagePrompt(parse_result, config_path, ui_language,
                               stdout_stream, stderr_stream);
  }

  const config::ResolvedConfig resolved_config =
      config::ResolveConfig(load_result.config,
                            ToConfigOverrides(parse_result.overrides));

  if (parse_result.command == cli::CommandKind::kModelsList) {
    WriteModelsList(load_result.config, resolved_config, ui_language,
                    stdout_stream);
    return 0;
  }

  if (parse_result.command == cli::CommandKind::kModelsSetDefault) {
    return SetDefaultModelCommand(parse_result, config_path, ui_language,
                                  stdout_stream, stderr_stream);
  }

  if (parse_result.command == cli::CommandKind::kAddModel) {
    return AddModelCommand(config_path, ui_language, stdin_stream,
                           stdout_stream, stderr_stream);
  }

  if (parse_result.command == cli::CommandKind::kHistoryList) {
    return ListHistoryCommand(history_path, ui_language, stdout_stream,
                              stderr_stream);
  }

  if (parse_result.command == cli::CommandKind::kHistoryClear) {
    return ClearHistoryCommand(history_path, ui_language, stdout_stream,
                               stderr_stream);
  }

  return TranslateCommand(parse_result, load_result.config, resolved_config,
                          prompt_registry, history_path, stdin_stream,
                          stdout_stream, stderr_stream);
}

}  // 命名空间 termtrans::app
