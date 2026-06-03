/**
 * @file test_args.cpp
 * @brief 测试命令行参数解析。
 */

#include "cli/args.h"

#include <iostream>
#include <string>

namespace {

/**
 * @brief 记录单个测试断言失败信息。
 *
 * @param condition 断言条件。
 * @param message 断言失败时写入 stderr 的说明。
 * @return 条件为 true 时返回 true。
 */
bool Expect(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << message << '\n';
    return false;
  }

  return true;
}

/**
 * @brief 验证长短帮助参数会短路为帮助请求。
 *
 * 帮助参数不依赖配置文件和后续命令形状，因此解析层必须直接返回
 * 成功。
 *
 * @return 成功时返回 0。
 */
int ShouldParseHelpFlags() {
  char program[] = "termtrans";
  char long_help[] = "--help";
  char* long_argv[] = {program, long_help};

  termtrans::cli::ParseResult result =
      termtrans::cli::ParseArgs(2, long_argv);
  if (!Expect(result.status == termtrans::cli::ParseStatus::kOk,
              "long help should parse") ||
      !Expect(result.show_help, "long help should request help")) {
    return 1;
  }

  char short_help[] = "-h";
  char* short_argv[] = {program, short_help};
  result = termtrans::cli::ParseArgs(2, short_argv);
  return Expect(result.status == termtrans::cli::ParseStatus::kOk,
                "short help should parse") &&
                 Expect(result.show_help, "short help should request help")
             ? 0
             : 1;
}

/**
 * @brief 验证翻译选项会写入 CLI 覆盖项。
 *
 * 当前只解析这些选项，不执行真实翻译；该用例防止后续改动破坏
 * `CLI 参数 > 配置文件` 的合并输入。
 *
 * @return 成功时返回 0。
 */
int ShouldParseTranslationOptions() {
  char program[] = "termtrans";
  char to[] = "--to";
  char language[] = "zh-CN";
  char model[] = "--model";
  char model_name[] = "deepseek";
  char no_stream[] = "--no-stream";
  char reuse_history[] = "--reuse-history";
  char text[] = "hello world";
  char* argv[] = {
      program, to, language, model, model_name, no_stream, reuse_history, text};

  const termtrans::cli::ParseResult result =
      termtrans::cli::ParseArgs(8, argv);

  return Expect(result.status == termtrans::cli::ParseStatus::kOk,
                "translation options should parse") &&
                 Expect(result.command == termtrans::cli::CommandKind::kTranslate,
                        "translation options should select translate command") &&
                 Expect(result.overrides.target_language == "zh-CN",
                        "--to should record target language") &&
                 Expect(result.overrides.model_name == "deepseek",
                        "--model should record model name") &&
                 Expect(result.overrides.stream == false,
                        "--no-stream should disable stream") &&
                 Expect(result.overrides.history_mode == "reuse",
                        "--reuse-history should override history mode") &&
                 Expect(result.positionals.size() == 1,
                        "one direct text positional should be collected") &&
                 Expect(result.positionals[0] == "hello world",
                        "direct text positional should match")
             ? 0
             : 1;
}

/**
 * @brief 验证多个历史策略 flag 同时出现时后者生效。
 *
 * CLI 用户可能通过 shell alias 叠加参数，解析层采用简单的后写覆盖规则。
 *
 * @return 成功时返回 0。
 */
int ShouldLetLaterHistoryFlagWin() {
  char program[] = "termtrans";
  char force[] = "--force";
  char no_history[] = "--no-history";
  char* argv[] = {program, force, no_history};

  const termtrans::cli::ParseResult result =
      termtrans::cli::ParseArgs(3, argv);

  return Expect(result.status == termtrans::cli::ParseStatus::kOk,
                "history flags should parse") &&
                 Expect(result.overrides.history_mode == "off",
                        "later history flag should win")
             ? 0
             : 1;
}

/**
 * @brief 验证完整 `config set <key> <value>` 命令形状。
 *
 * 解析层只保存键和值，配置值是否合法由配置模块负责判断。
 *
 * @return 成功时返回 0。
 */
int ShouldParseConfigSetCommand() {
  char program[] = "termtrans";
  char config[] = "config";
  char set[] = "set";
  char key[] = "history-mode";
  char value[] = "ask";
  char* argv[] = {program, config, set, key, value};

  const termtrans::cli::ParseResult result =
      termtrans::cli::ParseArgs(5, argv);

  return Expect(result.status == termtrans::cli::ParseStatus::kOk,
                "config set should parse") &&
                 Expect(result.command == termtrans::cli::CommandKind::kConfigSet,
                        "config set should select command") &&
                 Expect(result.config_key == "history-mode",
                        "config key should be recorded") &&
                 Expect(result.config_value == "ask",
                        "config value should be recorded")
             ? 0
             : 1;
}

/**
 * @brief 验证负数配置值不会在 CLI 层被误判为未知选项。
 *
 * `max-input-bytes -1` 必须流入配置校验，才能返回配置值非法而不是
 * 参数未知。
 *
 * @return 成功时返回 0。
 */
int ShouldParseNegativeConfigValueAsValue() {
  char program[] = "termtrans";
  char config[] = "config";
  char set[] = "set";
  char key[] = "max-input-bytes";
  char value[] = "-1";
  char* argv[] = {program, config, set, key, value};

  const termtrans::cli::ParseResult result =
      termtrans::cli::ParseArgs(5, argv);

  return Expect(result.status == termtrans::cli::ParseStatus::kOk,
                "negative config values should reach config validation") &&
                 Expect(result.config_value == "-1",
                        "negative config value should be preserved")
             ? 0
             : 1;
}

/**
 * @brief 验证 UI 语言列表命令形状。
 *
 * 该命令是当前已实际执行的基础信息命令。
 *
 * @return 成功时返回 0。
 */
int ShouldParseUiLanguagesListCommand() {
  char program[] = "termtrans";
  char ui_languages[] = "ui-languages";
  char list[] = "list";
  char* argv[] = {program, ui_languages, list};

  const termtrans::cli::ParseResult result =
      termtrans::cli::ParseArgs(3, argv);

  return Expect(result.status == termtrans::cli::ParseStatus::kOk,
                "ui-languages list should parse") &&
                 Expect(result.command ==
                            termtrans::cli::CommandKind::kUiLanguagesList,
                        "ui-languages list should select command")
             ? 0
             : 1;
}

/**
 * @brief 验证模型和历史命令已拥有稳定解析入口。
 *
 * 解析层只负责稳定命令形状；具体模型和历史行为由应用层执行。
 *
 * @return 成功时返回 0。
 */
int ShouldParseModelAndHistoryCommands() {
  char program[] = "termtrans";
  char add_model[] = "--add-model";
  char* add_model_argv[] = {program, add_model};
  termtrans::cli::ParseResult result =
      termtrans::cli::ParseArgs(2, add_model_argv);
  if (!Expect(result.command == termtrans::cli::CommandKind::kAddModel,
              "--add-model should select add model command")) {
    return 1;
  }

  char models[] = "models";
  char list[] = "list";
  char* models_argv[] = {program, models, list};
  result = termtrans::cli::ParseArgs(3, models_argv);
  if (!Expect(result.command == termtrans::cli::CommandKind::kModelsList,
              "models list should select command")) {
    return 1;
  }

  char set_default[] = "set-default";
  char model_name[] = "deepseek";
  char* set_default_argv[] = {program, models, set_default, model_name};
  result = termtrans::cli::ParseArgs(4, set_default_argv);
  if (!Expect(result.command == termtrans::cli::CommandKind::kModelsSetDefault,
              "models set-default should select command") ||
      !Expect(result.model_name == "deepseek",
              "models set-default should record model name")) {
    return 1;
  }

  char history[] = "history";
  char clear[] = "clear";
  char* history_argv[] = {program, history, clear};
  result = termtrans::cli::ParseArgs(3, history_argv);
  return Expect(result.command == termtrans::cli::CommandKind::kHistoryClear,
                "history clear should select command")
             ? 0
             : 1;
}

/**
 * @brief 验证目标语言和 Prompt 管理命令会解析为真实命令。
 *
 * 这些命令当前由应用层执行；解析层只校验命令形状并记录
 * language 与 prompt 文件路径。
 *
 * @return 成功时返回 0。
 */
int ShouldParseLanguageCommands() {
  char program[] = "termtrans";
  char languages[] = "languages";
  char list[] = "list";
  char* list_argv[] = {program, languages, list};
  termtrans::cli::ParseResult result =
      termtrans::cli::ParseArgs(3, list_argv);
  if (!Expect(result.command == termtrans::cli::CommandKind::kLanguagesList,
              "languages list should select list command")) {
    return 1;
  }

  char show[] = "show";
  char language[] = "zh-CN";
  char* languages_argv[] = {program, languages, show, language};
  result = termtrans::cli::ParseArgs(4, languages_argv);
  if (!Expect(result.command == termtrans::cli::CommandKind::kLanguagesShow,
              "languages show should select show command") ||
      !Expect(result.language == "zh-CN",
              "languages show should record language")) {
    return 1;
  }

  char set[] = "set";
  char custom_language[] = "pirate";
  char file[] = "--file";
  char prompt_path[] = "prompt.txt";
  char* languages_file_argv[] = {
      program, languages, set, custom_language, file, prompt_path};
  result = termtrans::cli::ParseArgs(6, languages_file_argv);
  if (!Expect(result.command == termtrans::cli::CommandKind::kLanguagesSet,
              "languages set --file should select set command") ||
      !Expect(result.language == "pirate",
              "languages set should record language") ||
      !Expect(result.prompt_file == "prompt.txt",
              "languages set --file should record prompt path")) {
    return 1;
  }

  char unset[] = "unset";
  char* unset_argv[] = {program, languages, unset, custom_language};
  result = termtrans::cli::ParseArgs(4, unset_argv);
  return Expect(result.command == termtrans::cli::CommandKind::kLanguagesUnset,
                "languages unset should select unset command") &&
                 Expect(result.language == "pirate",
                        "languages unset should record language")
             ? 0
             : 1;
}

/**
 * @brief 验证目标语言命令的错误形状会被拒绝。
 *
 * `languages set` 必须显式提供 `--file <path>`，其它目标语言命令不能携带
 * `--file`，避免产生文件读取范围歧义。
 *
 * @return 成功时返回 0。
 */
int ShouldRejectMalformedLanguageCommands() {
  char program[] = "termtrans";
  char languages[] = "languages";
  char set[] = "set";
  char custom_language[] = "pirate";
  char* missing_file_argv[] = {program, languages, set, custom_language};

  termtrans::cli::ParseResult result =
      termtrans::cli::ParseArgs(4, missing_file_argv);
  if (!Expect(result.status == termtrans::cli::ParseStatus::kError,
              "languages set without --file should fail") ||
      !Expect(result.error_kind == termtrans::cli::ParseErrorKind::kInvalidUsage,
              "missing prompt file should use invalid usage")) {
    return 1;
  }

  char show[] = "show";
  char language[] = "zh-CN";
  char file[] = "--file";
  char prompt_path[] = "prompt.txt";
  char* show_file_argv[] = {
      program, languages, show, language, file, prompt_path};
  result = termtrans::cli::ParseArgs(6, show_file_argv);
  return Expect(result.status == termtrans::cli::ParseStatus::kError,
                "languages show with --file should fail") &&
                 Expect(result.prompt_file == "prompt.txt",
                        "parser should still record rejected prompt path") &&
                 Expect(result.error_kind ==
                            termtrans::cli::ParseErrorKind::kInvalidUsage,
                        "show --file should use invalid usage")
             ? 0
             : 1;
}

/**
 * @brief 验证未知选项会被归类为未知参数错误。
 *
 * @return 成功时返回 0。
 */
int ShouldRejectUnknownOption() {
  char program[] = "termtrans";
  char unknown[] = "--unknown";
  char* argv[] = {program, unknown};

  const termtrans::cli::ParseResult result =
      termtrans::cli::ParseArgs(2, argv);

  return Expect(result.status == termtrans::cli::ParseStatus::kError,
                "unknown option should fail") &&
                 Expect(result.error_kind ==
                            termtrans::cli::ParseErrorKind::kUnknownArgument,
                        "unknown option should use unknown error kind") &&
                 Expect(result.error_argument == "--unknown",
                        "unknown option should be recorded")
             ? 0
             : 1;
}

/**
 * @brief 验证带值选项缺少取值时返回专用错误类别。
 *
 * @return 成功时返回 0。
 */
int ShouldRejectMissingOptionValue() {
  char program[] = "termtrans";
  char to[] = "--to";
  char* argv[] = {program, to};

  const termtrans::cli::ParseResult result =
      termtrans::cli::ParseArgs(2, argv);

  return Expect(result.status == termtrans::cli::ParseStatus::kError,
                "missing option value should fail") &&
                 Expect(result.error_kind ==
                            termtrans::cli::ParseErrorKind::kMissingValue,
                        "missing option value should use missing value error")
             ? 0
             : 1;
}

/**
 * @brief 验证命令形状错误和多个直接文本参数都会失败。
 *
 * 第一版要求 `termtrans hello world` 非法，用户必须用引号传入单个
 * 直接文本参数。
 *
 * @return 成功时返回 0。
 */
int ShouldRejectMalformedCommandsAndMultipleTextArgs() {
  char program[] = "termtrans";
  char first[] = "hello";
  char second[] = "world";
  char* text_argv[] = {program, first, second};

  termtrans::cli::ParseResult result =
      termtrans::cli::ParseArgs(3, text_argv);
  if (!Expect(result.status == termtrans::cli::ParseStatus::kError,
              "multiple direct text args should fail") ||
      !Expect(result.error_kind == termtrans::cli::ParseErrorKind::kInvalidUsage,
              "multiple text args should use invalid usage error")) {
    return 1;
  }

  char config[] = "config";
  char set[] = "set";
  char* config_argv[] = {program, config, set};
  result = termtrans::cli::ParseArgs(3, config_argv);
  return Expect(result.status == termtrans::cli::ParseStatus::kError,
                "malformed config command should fail") &&
                 Expect(result.error_kind ==
                            termtrans::cli::ParseErrorKind::kInvalidUsage,
                        "malformed config command should use invalid usage")
             ? 0
             : 1;
}

/**
 * @brief 验证 `--file` 只能用于 `languages ...` 命令族。
 *
 * `--file` 是 Prompt 管理专用选项，翻译命令不能误接受它。
 *
 * @return 成功时返回 0。
 */
int ShouldRejectLanguageFileOptionOutsideLanguageCommand() {
  char program[] = "termtrans";
  char file[] = "--file";
  char prompt_path[] = "prompt.txt";
  char text[] = "hello";
  char* argv[] = {program, file, prompt_path, text};

  const termtrans::cli::ParseResult result =
      termtrans::cli::ParseArgs(4, argv);

  return Expect(result.status == termtrans::cli::ParseStatus::kError,
                "--file outside languages command should fail") &&
                 Expect(result.error_kind ==
                            termtrans::cli::ParseErrorKind::kInvalidUsage,
                        "--file outside languages should use invalid usage")
             ? 0
             : 1;
}

}  // 匿名命名空间

/**
 * @brief 运行 CLI 解析测试。
 *
 * @return 任一用例失败时返回 1。
 */
int main() {
  if (ShouldParseHelpFlags() != 0) {
    return 1;
  }
  if (ShouldParseTranslationOptions() != 0) {
    return 1;
  }
  if (ShouldLetLaterHistoryFlagWin() != 0) {
    return 1;
  }
  if (ShouldParseConfigSetCommand() != 0) {
    return 1;
  }
  if (ShouldParseNegativeConfigValueAsValue() != 0) {
    return 1;
  }
  if (ShouldParseUiLanguagesListCommand() != 0) {
    return 1;
  }
  if (ShouldParseModelAndHistoryCommands() != 0) {
    return 1;
  }
  if (ShouldParseLanguageCommands() != 0) {
    return 1;
  }
  if (ShouldRejectMalformedLanguageCommands() != 0) {
    return 1;
  }
  if (ShouldRejectUnknownOption() != 0) {
    return 1;
  }
  if (ShouldRejectMissingOptionValue() != 0) {
    return 1;
  }
  if (ShouldRejectMalformedCommandsAndMultipleTextArgs() != 0) {
    return 1;
  }
  if (ShouldRejectLanguageFileOptionOutsideLanguageCommand() != 0) {
    return 1;
  }

  return 0;
}
