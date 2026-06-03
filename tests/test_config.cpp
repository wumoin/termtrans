/**
 * @file test_config.cpp
 * @brief 测试 TOML 配置读写、合并和默认配置路径。
 */

#include "config/config.h"
#include "config/platform_paths.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>

namespace {

/**
 * @brief 记录单个测试断言失败信息。
 *
 * @param condition 断言条件。
 * @param message 断言失败时写入 stderr 的说明。
 * @return 条件为 true 时返回 true。
 */
bool Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    return false;
  }

  return true;
}

/**
 * @brief 在测试期间临时修改环境变量，并在析构时恢复原始值。
 *
 * 配置路径和系统语言解析依赖 HOME、LC_ALL 等环境变量；该辅助类确保测试
 * 不会污染后续用例。
 *
 * @note 该类不是线程安全的。
 */
class EnvironmentVariableGuard {
 public:
  /**
   * @brief 保存环境变量原始值，供析构时恢复。
   *
   * @param name 环境变量名，调用方必须保证其生命周期长于本 guard。
   */
  explicit EnvironmentVariableGuard(const char* name)
      : name_(name), original_value_(Read(name)) {}

  /**
   * @brief 恢复构造时捕获的环境变量状态。
   *
   * 如果变量原本不存在，析构时会再次删除它。
   */
  ~EnvironmentVariableGuard() {
    if (original_value_.has_value()) {
      Set(name_, *original_value_);
      return;
    }

    Unset(name_);
  }

  /**
   * @brief 设置环境变量。
   *
   * @param name 环境变量名。
   * @param value 要写入的值。
   */
  static void Set(const char* name, const std::string& value) {
#ifdef _WIN32
    _putenv_s(name, value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
  }

  /**
   * @brief 删除环境变量。
   *
   * @param name 环境变量名。
   */
  static void Unset(const char* name) {
#ifdef _WIN32
    _putenv_s(name, "");
#else
    unsetenv(name);
#endif
  }

 private:
  /**
   * @brief 读取环境变量当前值。
   *
   * @param name 环境变量名。
   * @return 变量存在时返回其值；不存在时返回空 optional。
   */
  static std::optional<std::string> Read(const char* name) {
    const char* value = std::getenv(name);
    if (value == nullptr) {
      return std::nullopt;
    }

    return std::string(value);
  }

  const char* name_;  // 需要恢复的环境变量名。
  std::optional<std::string> original_value_;  // 测试修改前的原始值。
};

/**
 * @brief 创建配置测试使用的临时目录。
 *
 * @return 已创建的临时目录路径。
 */
std::filesystem::path MakeTempDirectory() {
  const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
  std::filesystem::path path =
      std::filesystem::temp_directory_path() /
      ("termtrans_config_test_" + std::to_string(tick));
  std::filesystem::create_directories(path);
  return path;
}

/**
 * @brief 写入测试文本文件。
 *
 * @param path 目标文件路径；父目录不存在时会创建。
 * @param text 要写入的完整文本。
 */
void WriteText(const std::filesystem::path& path, const std::string& text) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream output(path);
  output << text;
}

/**
 * @brief 读取测试文本文件。
 *
 * @param path 要读取的文件路径。
 * @return 文件完整内容；打开失败时返回空字符串。
 */
std::string ReadText(const std::filesystem::path& path) {
  std::ifstream input(path);
  return std::string(std::istreambuf_iterator<char>(input),
                     std::istreambuf_iterator<char>());
}

/**
 * @brief 验证缺失配置文件会被视为空配置。
 *
 * @return 成功时返回 0。
 */
int ShouldLoadMissingConfigAsEmptyConfig() {
  const std::filesystem::path config_path = MakeTempDirectory() / "missing.toml";
  const termtrans::config::LoadConfigResult result =
      termtrans::config::LoadConfig(config_path);

  return Expect(result.is_ok, "缺失配置文件应读取成功") &&
                 Expect(!result.config.ui_language.has_value(),
                        "缺失配置文件应返回空配置") &&
                 Expect(!result.config.target_language.has_value(),
                        "缺失配置文件不应产生目标语言配置") &&
                 Expect(!result.config.stream.has_value(),
                        "缺失配置文件不应产生 stream 配置")
             ? 0
             : 1;
}

/**
 * @brief 验证当前支持的顶层配置字段都能读取。
 *
 * @return 成功时返回 0。
 */
int ShouldLoadSupportedConfigFields() {
  const std::filesystem::path config_path = MakeTempDirectory() / "config.toml";
  WriteText(config_path,
            "default_model = \"deepseek\"\n"
            "target_language = \"zh-CN\"\n"
            "ui_language = \"en\"\n"
            "stream = false\n"
            "history_mode = \"reuse\"\n"
            "max_input_bytes = 4096\n");

  const termtrans::config::LoadConfigResult result =
      termtrans::config::LoadConfig(config_path);

  return Expect(result.is_ok, "完整配置应读取成功") &&
                 Expect(result.config.default_model == "deepseek",
                        "default_model 应读取") &&
                 Expect(result.config.target_language == "zh-CN",
                        "target_language 应读取") &&
                 Expect(result.config.ui_language == "en",
                        "ui_language 应读取") &&
                 Expect(result.config.stream == false, "stream 应读取") &&
                 Expect(result.config.history_mode == "reuse",
                        "history_mode 应读取") &&
                 Expect(result.config.max_input_bytes == 4096,
                        "max_input_bytes 应读取")
             ? 0
             : 1;
}

/**
 * @brief 验证配置 prompt 表会读取合法条目并忽略非法条目。
 *
 * 单个自定义 prompt 损坏不应影响其它 prompt 或顶层配置字段，这样用户
 * 手工编辑配置时更容易恢复。
 *
 * @return 成功时返回 0。
 */
int ShouldLoadPromptConfigFields() {
  const std::filesystem::path config_path = MakeTempDirectory() / "config.toml";
  WriteText(config_path,
            "target_language = \"zh-CN\"\n"
            "\n"
            "[prompts.\"zh-CN\"]\n"
            "prompt = \"configured zh prompt\"\n"
            "\n"
            "[prompts.\"pirate\"]\n"
            "prompt = \"configured pirate prompt\"\n"
            "\n"
            "[prompts.\"bad language\"]\n"
            "prompt = \"bad code prompt\"\n"
            "\n"
            "[prompts.\"empty\"]\n"
            "prompt = \"   \"\n"
            "\n"
            "[prompts.\"missing\"]\n"
            "other = \"not a prompt\"\n");

  const termtrans::config::LoadConfigResult result =
      termtrans::config::LoadConfig(config_path);

  return Expect(result.is_ok, "包含 prompts 的配置应读取成功") &&
                 Expect(result.config.target_language == "zh-CN",
                        "prompts 不应影响顶层 target_language") &&
                 Expect(result.config.prompts.size() == 2,
                        "只应读取合法且非空的 prompt 条目") &&
                 Expect(result.config.prompts.at("zh-CN") ==
                            "configured zh prompt",
                        "应读取内置语言覆盖 prompt") &&
                 Expect(result.config.prompts.at("pirate") ==
                            "configured pirate prompt",
                        "应读取自定义语言 prompt")
             ? 0
             : 1;
}

/**
 * @brief 验证单个非法字段会被忽略而不是使整个配置读取失败。
 *
 * 该行为保证用户手工编辑配置时，一个字段拼写错误不会阻止其它有效
 * 配置生效。
 *
 * @return 成功时返回 0。
 */
int ShouldIgnoreInvalidConfigValues() {
  const std::filesystem::path config_path = MakeTempDirectory() / "config.toml";
  WriteText(config_path,
            "default_model = \"\"\n"
            "target_language = \"简体中文\"\n"
            "ui_language = \"ja\"\n"
            "stream = \"yes\"\n"
            "history_mode = \"always\"\n"
            "max_input_bytes = -1\n");

  const termtrans::config::LoadConfigResult result =
      termtrans::config::LoadConfig(config_path);

  return Expect(result.is_ok, "非法字段不应导致配置读取失败") &&
                 Expect(!result.config.default_model.has_value(),
                        "非法 default_model 应忽略") &&
                 Expect(!result.config.target_language.has_value(),
                        "非法 target_language 应忽略") &&
                 Expect(!result.config.ui_language.has_value(),
                        "非法 ui_language 应忽略") &&
                 Expect(!result.config.stream.has_value(),
                        "非法 stream 应忽略") &&
                 Expect(!result.config.history_mode.has_value(),
                        "非法 history_mode 应忽略") &&
                 Expect(!result.config.max_input_bytes.has_value(),
                        "非法 max_input_bytes 应忽略")
             ? 0
             : 1;
}

/**
 * @brief 验证目标语言和界面语言的配置优先级与系统语言兜底。
 *
 * 该用例会修改 locale 环境变量，因此必须通过 guard 在结束时恢复。
 *
 * @return 成功时返回 0。
 */
int ShouldResolveLanguagesFromConfigAndSystem() {
  EnvironmentVariableGuard lc_all_guard("LC_ALL");
  EnvironmentVariableGuard lc_messages_guard("LC_MESSAGES");
  EnvironmentVariableGuard lang_guard("LANG");

  termtrans::config::AppConfig config;
  config.ui_language = "zh-CN";
  config.target_language = "ja";
  if (!Expect(termtrans::config::ResolveUiLanguage(config) == "zh-CN",
              "合法中文 UI 配置应优先解析为 zh-CN") ||
      !Expect(termtrans::config::ResolveTargetLanguage(config) == "ja",
              "合法目标语言配置应优先解析")) {
    return 1;
  }

  config = termtrans::config::AppConfig{};
  EnvironmentVariableGuard::Set("LC_ALL", "zh_CN.UTF-8");
  EnvironmentVariableGuard::Unset("LC_MESSAGES");
  EnvironmentVariableGuard::Unset("LANG");
  if (!Expect(termtrans::config::ResolveUiLanguage(config) == "zh-CN",
              "中文系统语言应解析为 zh-CN UI") ||
      !Expect(termtrans::config::ResolveTargetLanguage(config) == "zh-CN",
              "中文系统语言应解析为 zh-CN 目标语言")) {
    return 1;
  }

  EnvironmentVariableGuard::Set("LC_ALL", "fr_FR.UTF-8");
  return Expect(termtrans::config::ResolveUiLanguage(config) == "en",
                "非中文系统语言应解析为 en UI") &&
                 Expect(termtrans::config::ResolveTargetLanguage(config) == "en",
                        "非中文系统语言应解析为 en 目标语言")
             ? 0
             : 1;
}

/**
 * @brief 验证 CLI 覆盖项优先于配置文件值。
 *
 * @return 成功时返回 0。
 */
int ShouldResolveConfigWithCliOverrides() {
  termtrans::config::AppConfig config;
  config.default_model = "deepseek";
  config.target_language = "zh-CN";
  config.ui_language = "en";
  config.stream = true;
  config.history_mode = "ask";
  config.max_input_bytes = 128;

  termtrans::config::ConfigOverrides overrides;
  overrides.target_language = "ja";
  overrides.model_name = "local-qwen";
  overrides.stream = false;
  overrides.history_mode = "force";

  const termtrans::config::ResolvedConfig resolved =
      termtrans::config::ResolveConfig(config, overrides);

  return Expect(resolved.default_model == "local-qwen",
                "CLI model should override default model") &&
                 Expect(resolved.target_language == "ja",
                        "CLI target language should override config") &&
                 Expect(resolved.ui_language == "en",
                        "UI language should come from config") &&
                 Expect(!resolved.stream, "CLI stream should override config") &&
                 Expect(resolved.history_mode == "force",
                        "CLI history mode should override config") &&
                 Expect(resolved.max_input_bytes == 128,
                        "max_input_bytes should come from config")
             ? 0
             : 1;
}

/**
 * @brief 验证 `config set` 支持的所有字段都能写入并重新读取。
 *
 * @return 成功时返回 0。
 */
int ShouldSaveAndReloadConfigValues() {
  const std::filesystem::path config_path =
      MakeTempDirectory() / "termtrans" / "config.toml";

  const termtrans::config::SaveConfigResult target_language_result =
      termtrans::config::SaveConfigValue(config_path, "target-language",
                                         "zh-CN");
  const termtrans::config::SaveConfigResult ui_language_result =
      termtrans::config::SaveConfigValue(config_path, "ui-language", "en");
  const termtrans::config::SaveConfigResult stream_result =
      termtrans::config::SaveConfigValue(config_path, "stream", "false");
  const termtrans::config::SaveConfigResult history_result =
      termtrans::config::SaveConfigValue(config_path, "history-mode", "off");
  const termtrans::config::SaveConfigResult max_input_result =
      termtrans::config::SaveConfigValue(config_path, "max-input-bytes", "0");

  const termtrans::config::LoadConfigResult load_result =
      termtrans::config::LoadConfig(config_path);

  return Expect(target_language_result.is_ok,
                "写入 target-language 应成功") &&
                 Expect(ui_language_result.is_ok,
                        "写入 ui-language 应成功") &&
                 Expect(stream_result.is_ok, "写入 stream 应成功") &&
                 Expect(history_result.is_ok,
                        "写入 history-mode 应成功") &&
                 Expect(max_input_result.is_ok,
                        "写入 max-input-bytes 应成功") &&
                 Expect(load_result.is_ok, "写入后应能重新读取配置") &&
                 Expect(load_result.config.target_language == "zh-CN",
                        "重新读取时应得到写入的 target_language") &&
                 Expect(load_result.config.ui_language == "en",
                        "重新读取时应得到写入的 ui_language") &&
                 Expect(load_result.config.stream == false,
                        "重新读取时应得到写入的 stream") &&
                 Expect(load_result.config.history_mode == "off",
                        "重新读取时应得到写入的 history_mode") &&
                 Expect(load_result.config.max_input_bytes == 0,
                        "重新读取时应得到写入的 max_input_bytes")
             ? 0
             : 1;
}

/**
 * @brief 验证 prompt 写入、读取和删除流程。
 *
 * prompt 写入应保留其它顶层配置；unset 删除已有配置时 did_change 为 true，
 * 重复 unset 时返回成功且 did_change 为 false。
 *
 * @return 成功时返回 0。
 */
int ShouldSaveReloadAndUnsetPrompts() {
  const std::filesystem::path config_path =
      MakeTempDirectory() / "termtrans" / "config.toml";

  const termtrans::config::SaveConfigResult stream_result =
      termtrans::config::SaveConfigValue(config_path, "stream", "false");
  const termtrans::config::SaveConfigResult zh_result =
      termtrans::config::SavePrompt(config_path, "zh-CN",
                                    "custom zh prompt");
  const termtrans::config::SaveConfigResult pirate_result =
      termtrans::config::SavePrompt(config_path, "pirate",
                                    "custom pirate prompt");
  const termtrans::config::LoadConfigResult load_result =
      termtrans::config::LoadConfig(config_path);

  if (!Expect(stream_result.is_ok, "写入 stream 应成功") ||
      !Expect(zh_result.is_ok, "写入内置语言 prompt 应成功") ||
      !Expect(pirate_result.is_ok, "写入自定义语言 prompt 应成功") ||
      !Expect(load_result.is_ok, "写入 prompt 后应能读取配置") ||
      !Expect(load_result.config.stream == false,
              "写入 prompt 不应破坏已有 stream 配置") ||
      !Expect(load_result.config.prompts.at("zh-CN") == "custom zh prompt",
              "应读取写入的 zh-CN prompt") ||
      !Expect(load_result.config.prompts.at("pirate") ==
                  "custom pirate prompt",
              "应读取写入的 pirate prompt")) {
    return 1;
  }

  const termtrans::config::SaveConfigResult unset_zh_result =
      termtrans::config::UnsetPrompt(config_path, "zh-CN");
  const termtrans::config::SaveConfigResult unset_zh_again_result =
      termtrans::config::UnsetPrompt(config_path, "zh-CN");
  const termtrans::config::SaveConfigResult unset_pirate_result =
      termtrans::config::UnsetPrompt(config_path, "pirate");
  const termtrans::config::LoadConfigResult final_load_result =
      termtrans::config::LoadConfig(config_path);

  return Expect(unset_zh_result.is_ok, "删除已有 zh-CN prompt 应成功") &&
                 Expect(unset_zh_result.did_change,
                        "删除已有 prompt 应标记 did_change") &&
                 Expect(unset_zh_again_result.is_ok,
                        "重复删除 prompt 应成功") &&
                 Expect(!unset_zh_again_result.did_change,
                        "重复删除缺失 prompt 不应标记 did_change") &&
                 Expect(unset_pirate_result.is_ok,
                        "删除 pirate prompt 应成功") &&
                 Expect(final_load_result.is_ok,
                        "删除 prompt 后应能读取配置") &&
                 Expect(final_load_result.config.prompts.empty(),
                        "所有 prompt 删除后配置 prompt 集合应为空") &&
                 Expect(final_load_result.config.stream == false,
                        "删除 prompt 不应破坏顶层 stream 配置")
             ? 0
             : 1;
}

/**
 * @brief 验证写入单个字段时会保留配置文件中的其它字段。
 *
 * 该用例保护 `SaveConfigValue()` 的非破坏性写回语义。
 *
 * @return 成功时返回 0。
 */
int ShouldPreserveUnrelatedConfigValuesOnSave() {
  const std::filesystem::path config_path = MakeTempDirectory() / "config.toml";
  WriteText(config_path,
            "default_model = \"deepseek\"\n"
            "target_language = \"en\"\n");

  const termtrans::config::SaveConfigResult save_result =
      termtrans::config::SaveConfigValue(config_path, "ui-language", "zh-CN");
  const std::string text = ReadText(config_path);

  return Expect(save_result.is_ok, "写入 ui-language 应成功") &&
                 Expect(text.find("default_model") != std::string::npos,
                        "写入单个字段时应保留 default_model") &&
                 Expect(text.find("target_language") != std::string::npos,
                        "写入单个字段时应保留 target_language") &&
                 Expect(text.find("ui_language") != std::string::npos,
                        "写入单个字段时应新增 ui_language")
             ? 0
             : 1;
}

/**
 * @brief 验证非法配置键和值会在写入阶段失败。
 *
 * @return 成功时返回 0。
 */
int ShouldRejectInvalidConfigWrites() {
  const std::filesystem::path config_path = MakeTempDirectory() / "config.toml";

  const bool is_valid =
      Expect(!termtrans::config::SaveConfigValue(config_path, "ui-language",
                                                 "ja")
                  .is_ok,
             "非法 UI 语言写入应失败") &&
      Expect(!termtrans::config::SaveConfigValue(config_path, "stream", "yes")
                  .is_ok,
             "非法 stream 写入应失败") &&
      Expect(!termtrans::config::SaveConfigValue(config_path, "history-mode",
                                                 "always")
                  .is_ok,
             "非法 history-mode 写入应失败") &&
      Expect(!termtrans::config::SaveConfigValue(config_path, "max-input-bytes",
                                                 "-1")
                  .is_ok,
             "非法 max-input-bytes 写入应失败") &&
      Expect(!termtrans::config::SaveConfigValue(config_path, "unknown", "x")
                  .is_ok,
             "未知配置键写入应失败");

  return is_valid ? 0 : 1;
}

/**
 * @brief 验证非法 prompt 写入和删除会失败。
 *
 * @return 成功时返回 0。
 */
int ShouldRejectInvalidPromptWrites() {
  const std::filesystem::path config_path = MakeTempDirectory() / "config.toml";

  const bool is_valid =
      Expect(!termtrans::config::SavePrompt(config_path, "简体中文",
                                            "prompt")
                  .is_ok,
             "非 ASCII 语言 code 写入 prompt 应失败") &&
      Expect(!termtrans::config::SavePrompt(config_path, "zh-CN", "   ")
                  .is_ok,
             "空白 prompt 写入应失败") &&
      Expect(!termtrans::config::UnsetPrompt(config_path, "bad code").is_ok,
             "非法语言 code 删除 prompt 应失败");

  return is_valid ? 0 : 1;
}

/**
 * @brief 验证 Linux 默认配置路径固定为 HOME 下的 `.config`。
 *
 * 该路径规则是阶段 1 已确认的仓库约束，不读取 XDG_CONFIG_HOME。
 *
 * @return 成功时返回 0；非 Linux 平台跳过并返回 0。
 */
int ShouldUseHomeConfigPathOnLinux() {
#if !defined(_WIN32) && !defined(__APPLE__)
  const std::filesystem::path home = MakeTempDirectory();
  const std::filesystem::path xdg = MakeTempDirectory();
  EnvironmentVariableGuard home_guard("HOME");
  EnvironmentVariableGuard xdg_guard("XDG_CONFIG_HOME");
  EnvironmentVariableGuard::Set("HOME", home.string());
  EnvironmentVariableGuard::Set("XDG_CONFIG_HOME", xdg.string());

  const std::filesystem::path expected =
      home / ".config" / "termtrans" / "config.toml";
  return Expect(termtrans::config::DefaultConfigPath() == expected,
                "Linux 默认配置路径必须固定使用 HOME/.config")
             ? 0
             : 1;
#else
  return 0;
#endif
}

/**
 * @brief 验证历史数据库路径与配置文件同目录。
 *
 * @return 成功时返回 0。
 */
int ShouldPlaceHistoryPathBesideConfigPath() {
  const std::filesystem::path config_path =
      termtrans::config::DefaultConfigPath();
  const std::filesystem::path history_path =
      termtrans::config::DefaultHistoryPath();

  return Expect(history_path.parent_path() == config_path.parent_path(),
                "历史数据库必须与 config.toml 位于同一目录") &&
                 Expect(history_path.filename() == "history.sqlite",
                        "历史数据库文件名应为 history.sqlite")
             ? 0
             : 1;
}

}  // 匿名命名空间

/**
 * @brief 运行配置模块测试。
 *
 * @return 任一用例失败时返回 1。
 */
int main() {
  if (ShouldLoadMissingConfigAsEmptyConfig() != 0) {
    return 1;
  }
  if (ShouldLoadSupportedConfigFields() != 0) {
    return 1;
  }
  if (ShouldLoadPromptConfigFields() != 0) {
    return 1;
  }
  if (ShouldIgnoreInvalidConfigValues() != 0) {
    return 1;
  }
  if (ShouldResolveLanguagesFromConfigAndSystem() != 0) {
    return 1;
  }
  if (ShouldResolveConfigWithCliOverrides() != 0) {
    return 1;
  }
  if (ShouldSaveAndReloadConfigValues() != 0) {
    return 1;
  }
  if (ShouldSaveReloadAndUnsetPrompts() != 0) {
    return 1;
  }
  if (ShouldPreserveUnrelatedConfigValuesOnSave() != 0) {
    return 1;
  }
  if (ShouldRejectInvalidConfigWrites() != 0) {
    return 1;
  }
  if (ShouldRejectInvalidPromptWrites() != 0) {
    return 1;
  }
  if (ShouldUseHomeConfigPathOnLinux() != 0) {
    return 1;
  }
  if (ShouldPlaceHistoryPathBesideConfigPath() != 0) {
    return 1;
  }

  return 0;
}
