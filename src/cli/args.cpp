/**
 * @file args.cpp
 * @brief 实现命令行参数解析。
 */

#include "cli/args.h"

#include <string>
#include <string_view>

namespace termtrans::cli {
namespace {

/**
 * @brief 将解析结果切换为失败状态。
 *
 * 所有解析错误都通过该函数设置，避免调用方只记录错误参数而遗漏错误
 * 类别。
 *
 * @param result 要更新的解析结果，不能为空。
 * @param error_kind 用于后续本地化诊断文案的错误类别。
 * @param argument 触发错误的原始参数或命令词。
 */
void SetError(ParseResult* result,
              ParseErrorKind error_kind,
              std::string_view argument) {
  result->status = ParseStatus::kError;
  result->error_kind = error_kind;
  result->error_argument = std::string(argument);
}

/**
 * @brief 判断当前解析位置是否正等待 `config set` 的配置值。
 *
 * `config set max-input-bytes -1` 这类调用需要把 `-1` 当作配置值交给
 * 配置层校验，而不是在参数层提前归类为未知选项。
 *
 * @param positionals 已收集的位置参数。
 * @return 正好收集到 `config set <key>` 时返回 true。
 */
bool NeedsConfigSetValue(const std::vector<std::string>& positionals) {
  return positionals.size() == 3 && positionals[0] == "config" &&
         positionals[1] == "set";
}

/**
 * @brief 判断参数外观是否类似 CLI 选项。
 *
 * @param argument 原始参数文本。
 * @return 以 `-` 开头且长度大于 1 时返回 true。
 */
bool IsOptionLike(std::string_view argument) {
  return argument.size() > 1 && argument[0] == '-';
}

/**
 * @brief 记录带值选项的取值。
 *
 * 该函数只负责把已确认存在的值写入解析结果，不判断值是否业务合法；
 * 例如目标语言是否存在会在 Prompt 阶段处理。
 *
 * @param result 要更新的解析结果，不能为空。
 * @param option 选项名。
 * @param value 选项取值。
 * @return 识别并写入成功时返回 true。
 */
bool AssignOptionValue(ParseResult* result,
                       std::string_view option,
                       std::string_view value) {
  if (option == "--to") {
    result->overrides.target_language = std::string(value);
    return true;
  }

  if (option == "--model") {
    result->overrides.model_name = std::string(value);
    return true;
  }

  if (option == "--file") {
    result->prompt_file = std::string(value);
    return true;
  }

  return false;
}

/**
 * @brief 判断参数是否为当前阶段已知的带值选项。
 *
 * `--file` 只服务 `languages set ... --file` 入口，后续会在命令形状
 * 解析完成后再次限制其使用范围。
 *
 * @param argument 原始参数文本。
 * @return 属于已知带值选项时返回 true。
 */
bool IsKnownValueOption(std::string_view argument) {
  return argument == "--to" || argument == "--model" || argument == "--file";
}

/**
 * @brief 解析不带值的选项。
 *
 * 历史相关 flag 只覆盖最终配置，不在本阶段触发任何历史查询或写入。
 * 多个同类 flag 同时出现时，后出现的值覆盖先出现的值。
 *
 * @param result 要更新的解析结果，不能为空。
 * @param argument 原始参数文本。
 * @return 当前参数被识别为 flag 时返回 true。
 */
bool ParseFlag(ParseResult* result, std::string_view argument) {
  if (argument == "--stream") {
    result->overrides.stream = true;
    return true;
  }

  if (argument == "--no-stream") {
    result->overrides.stream = false;
    return true;
  }

  if (argument == "--force") {
    result->overrides.history_mode = "force";
    return true;
  }

  if (argument == "--reuse-history") {
    result->overrides.history_mode = "reuse";
    return true;
  }

  if (argument == "--no-history") {
    result->overrides.history_mode = "off";
    return true;
  }

  if (argument == "--add-model") {
    result->command = CommandKind::kAddModel;
    return true;
  }

  return false;
}

/**
 * @brief 判断位置参数是否构成 `config set <key> <value>`。
 *
 * @param positionals 已收集的位置参数。
 * @return 命令形状完整时返回 true；配置键和值是否合法不在此处判断。
 */
bool IsConfigSetCommand(const std::vector<std::string>& positionals) {
  return positionals.size() == 4 && positionals[0] == "config" &&
         positionals[1] == "set";
}

/**
 * @brief 判断位置参数是否构成 `ui-languages list`。
 *
 * @param positionals 已收集的位置参数。
 * @return 命令形状完整时返回 true。
 */
bool IsUiLanguagesListCommand(const std::vector<std::string>& positionals) {
  return positionals.size() == 2 && positionals[0] == "ui-languages" &&
         positionals[1] == "list";
}

/**
 * @brief 判断位置参数是否构成 `models list`。
 *
 * @param positionals 已收集的位置参数。
 * @return 命令形状完整时返回 true。
 */
bool IsModelsListCommand(const std::vector<std::string>& positionals) {
  return positionals.size() == 2 && positionals[0] == "models" &&
         positionals[1] == "list";
}

/**
 * @brief 判断位置参数是否构成 `models set-default <name>`。
 *
 * @param positionals 已收集的位置参数。
 * @return 命令形状完整时返回 true。
 */
bool IsModelsSetDefaultCommand(const std::vector<std::string>& positionals) {
  return positionals.size() == 3 && positionals[0] == "models" &&
         positionals[1] == "set-default";
}

/**
 * @brief 判断位置参数是否构成 `history list`。
 *
 * @param positionals 已收集的位置参数。
 * @return 命令形状完整时返回 true。
 */
bool IsHistoryListCommand(const std::vector<std::string>& positionals) {
  return positionals.size() == 2 && positionals[0] == "history" &&
         positionals[1] == "list";
}

/**
 * @brief 判断位置参数是否构成 `history clear`。
 *
 * @param positionals 已收集的位置参数。
 * @return 命令形状完整时返回 true。
 */
bool IsHistoryClearCommand(const std::vector<std::string>& positionals) {
  return positionals.size() == 2 && positionals[0] == "history" &&
         positionals[1] == "clear";
}

/**
 * @brief 判断位置参数是否构成 `languages list`。
 *
 * @param positionals 已收集的位置参数。
 * @return 命令形状完整时返回 true。
 */
bool IsLanguagesListCommand(const std::vector<std::string>& positionals) {
  return positionals.size() == 2 && positionals[0] == "languages" &&
         positionals[1] == "list";
}

/**
 * @brief 判断位置参数是否构成 `languages show <language>`。
 *
 * @param positionals 已收集的位置参数。
 * @return 命令形状完整时返回 true。
 */
bool IsLanguagesShowCommand(const std::vector<std::string>& positionals) {
  return positionals.size() == 3 && positionals[0] == "languages" &&
         positionals[1] == "show";
}

/**
 * @brief 判断位置参数是否构成 `languages set <language>`。
 *
 * `--file <path>` 属于带值选项，不会进入 positionals；是否已经提供
 * prompt 文件路径会在命令形状解析后单独校验。
 *
 * @param positionals 已收集的位置参数。
 * @return 命令主体形状完整时返回 true。
 */
bool IsLanguagesSetCommand(const std::vector<std::string>& positionals) {
  return positionals.size() == 3 && positionals[0] == "languages" &&
         positionals[1] == "set";
}

/**
 * @brief 判断位置参数是否构成 `languages unset <language>`。
 *
 * @param positionals 已收集的位置参数。
 * @return 命令形状完整时返回 true。
 */
bool IsLanguagesUnsetCommand(const std::vector<std::string>& positionals) {
  return positionals.size() == 3 && positionals[0] == "languages" &&
         positionals[1] == "unset";
}

/**
 * @brief 判断单个位置参数是否是保留命令族前缀。
 *
 * 保留前缀不能被当成直接文本翻译，否则 `termtrans config` 会被误解释成
 * 翻译字面文本 `config`。
 *
 * @param command 第一个位置参数。
 * @return 属于保留命令族时返回 true。
 */
bool IsReservedCommandPrefix(std::string_view command) {
  return command == "config" || command == "ui-languages" ||
         command == "models" || command == "history" ||
         command == "languages";
}

/**
 * @brief 根据位置参数确定顶层命令。
 *
 * 输入来源规则要求一个普通位置参数表示直接文本，多个普通位置参数
 * 属于用法错误；命令族前缀必须匹配完整命令形状。
 *
 * @param result 已收集选项和位置参数的解析结果，不能为空。
 */
void ResolvePositionalCommand(ParseResult* result) {
  const std::vector<std::string>& positionals = result->positionals;

  if (result->command == CommandKind::kAddModel) {
    if (!positionals.empty()) {
      SetError(result, ParseErrorKind::kInvalidUsage, "--add-model");
    }
    return;
  }

  if (positionals.empty()) {
    result->command = CommandKind::kTranslate;
    return;
  }

  if (IsConfigSetCommand(positionals)) {
    result->command = CommandKind::kConfigSet;
    result->config_key = positionals[2];
    result->config_value = positionals[3];
    return;
  }

  if (IsUiLanguagesListCommand(positionals)) {
    result->command = CommandKind::kUiLanguagesList;
    return;
  }

  if (IsModelsListCommand(positionals)) {
    result->command = CommandKind::kModelsList;
    return;
  }

  if (IsModelsSetDefaultCommand(positionals)) {
    result->command = CommandKind::kModelsSetDefault;
    result->model_name = positionals[2];
    return;
  }

  if (IsHistoryListCommand(positionals)) {
    result->command = CommandKind::kHistoryList;
    return;
  }

  if (IsHistoryClearCommand(positionals)) {
    result->command = CommandKind::kHistoryClear;
    return;
  }

  if (IsLanguagesListCommand(positionals)) {
    result->command = CommandKind::kLanguagesList;
    return;
  }

  if (IsLanguagesShowCommand(positionals)) {
    result->command = CommandKind::kLanguagesShow;
    result->language = positionals[2];
    return;
  }

  if (IsLanguagesSetCommand(positionals)) {
    result->command = CommandKind::kLanguagesSet;
    result->language = positionals[2];
    return;
  }

  if (IsLanguagesUnsetCommand(positionals)) {
    result->command = CommandKind::kLanguagesUnset;
    result->language = positionals[2];
    return;
  }

  if (positionals.size() == 1 && !IsReservedCommandPrefix(positionals[0])) {
    result->command = CommandKind::kTranslate;
    return;
  }

  SetError(result, ParseErrorKind::kInvalidUsage, positionals[0]);
}

/**
 * @brief 限制 `--file` 只能用于 `languages set`。
 *
 * 如果在其它命令中接受 `--file`，会让用户误以为应用会从文件读取
 * 翻译输入或其它配置值。
 *
 * @param result 已完成命令形状解析的结果，不能为空。
 */
void RejectLanguageOnlyOptionsOutsideLanguageCommands(ParseResult* result) {
  if (!result->prompt_file.empty() &&
      result->command != CommandKind::kLanguagesSet) {
    SetError(result, ParseErrorKind::kInvalidUsage, "--file");
  }
}

/**
 * @brief 校验 `languages set` 必须携带 prompt 文件路径。
 *
 * @param result 已完成命令形状解析的结果，不能为空。
 */
void RequirePromptFileForLanguagesSet(ParseResult* result) {
  if (result->command == CommandKind::kLanguagesSet &&
      result->prompt_file.empty()) {
    SetError(result, ParseErrorKind::kInvalidUsage, "languages set");
  }
}

}  // 匿名命名空间

/**
 * @brief 解析进程参数并生成稳定命令结构。
 *
 * 解析过程不访问文件系统、不读取 stdin，也不判断配置值是否存在于后续
 * 模型表或 Prompt 表；它只负责把 CLI 表面转换成应用层可分发的数据
 * 结构。
 *
 * @param argc main() 传入的参数数量。
 * @param argv main() 传入的参数数组；argv[0] 会被跳过。
 * @return 解析结果。错误结果会包含错误类别和触发错误的原始参数。
 */
ParseResult ParseArgs(int argc, char* argv[]) {
  ParseResult result;

  for (int index = 1; index < argc; ++index) {
    const std::string_view argument = argv[index] == nullptr ? "" : argv[index];

    if (argument == "--help" || argument == "-h") {
      result.show_help = true;
      return result;
    }

    if (IsKnownValueOption(argument)) {
      if (index + 1 >= argc) {
        SetError(&result, ParseErrorKind::kMissingValue, argument);
        return result;
      }

      const std::string_view value =
          argv[index + 1] == nullptr ? "" : argv[index + 1];
      if (IsOptionLike(value)) {
        SetError(&result, ParseErrorKind::kMissingValue, argument);
        return result;
      }

      AssignOptionValue(&result, argument, value);
      ++index;
      continue;
    }

    if (ParseFlag(&result, argument)) {
      continue;
    }

    if (IsOptionLike(argument) && !NeedsConfigSetValue(result.positionals)) {
      SetError(&result, ParseErrorKind::kUnknownArgument, argument);
      return result;
    }

    result.positionals.emplace_back(argument);
  }

  ResolvePositionalCommand(&result);
  if (result.status == ParseStatus::kOk) {
    RejectLanguageOnlyOptionsOutsideLanguageCommands(&result);
  }
  if (result.status == ParseStatus::kOk) {
    RequirePromptFileForLanguagesSet(&result);
  }

  return result;
}

}  // 命名空间 termtrans::cli
