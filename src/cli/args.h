/**
 * @file args.h
 * @brief 声明命令行参数解析。
 */

#pragma once

#include <optional>
#include <string>
#include <vector>

namespace termtrans::cli {

/**
 * @brief 一次命令行调用的解析状态。
 */
enum class ParseStatus {
  kOk,     // 参数已解析完成，可继续交给应用层分发。
  kError,  // 参数存在结构错误，应用层应输出诊断并停止执行。
};

/**
 * @brief 当前支持解析的顶层命令。
 */
enum class CommandKind {
  kTranslate,              // 翻译主流程；当前仍返回不可用提示。
  kConfigSet,              // 写入一个顶层配置字段。
  kUiLanguagesList,        // 列出当前支持的终端界面语言。
  kAddModel,               // 交互式模型添加流程。
  kModelsList,             // 列出模型配置。
  kModelsSetDefault,       // 设置默认模型配置。
  kHistoryList,            // 列出历史记录元数据。
  kHistoryClear,           // 清空历史记录。
  kLanguagesList,          // 列出可用目标语言和 prompt 来源。
  kLanguagesShow,          // 展示一个目标语言当前生效的完整 prompt。
  kLanguagesSet,           // 从文件写入一个目标语言的完整 prompt。
  kLanguagesUnset,         // 删除一个目标语言的配置 prompt。
};

/**
 * @brief 解析失败时用于选择用户可见诊断文案的错误类别。
 */
enum class ParseErrorKind {
  kNone,             // 未发生解析错误。
  kUnknownArgument,  // 选项或命令词不在当前支持范围内。
  kMissingValue,     // 带值选项缺少后续取值。
  kInvalidUsage,     // 命令形状错误或位置参数数量非法。
};

/**
 * @brief 一次翻译调用中由 CLI 参数覆盖配置文件的字段。
 *
 * 该结构只描述参数层面的覆盖意图，不负责校验配置值是否存在于模型表
 * 或 prompt 表中。
 */
struct CliOverrides {
  // 本次翻译的目标语言覆盖值；解析层只要求是稳定 ASCII 标识。
  std::optional<std::string> target_language;
  // 本次翻译使用的本地模型配置名称；模型是否存在留给模型阶段校验。
  std::optional<std::string> model_name;
  // 本次翻译是否启用流式输出。
  std::optional<bool> stream;
  // 本次翻译的历史策略覆盖值：force、reuse 或 off。
  std::optional<std::string> history_mode;
};

/**
 * @brief 命令行的解析结果。
 *
 * 解析层只验证 CLI 表面形状，不访问文件系统、不读取配置，也不判断
 * 模型或目标语言是否真实存在。
 */
struct ParseResult {
  ParseStatus status = ParseStatus::kOk;  // 解析是否完成且可分发。
  ParseErrorKind error_kind = ParseErrorKind::kNone;  // 失败时的诊断类别。
  CommandKind command = CommandKind::kTranslate;  // 当前调用的顶层命令。
  bool show_help = false;  // 帮助参数会短路其它命令解析。
  CliOverrides overrides;  // 翻译调用中的 CLI 配置覆盖。
  std::vector<std::string> positionals;  // 命令词或一个直接文本参数。
  std::string config_key;  // `config set` 使用的 CLI 键名。
  std::string config_value;  // `config set` 使用的原始值文本。
  std::string model_name;  // `models set-default` 的本地模型配置名称。
  std::string language;  // `languages` 子命令使用的目标语言标识。
  std::string prompt_file;  // `languages set ... --file` 的 prompt 文件路径。
  std::string error_argument;  // 触发解析错误的参数或命令词。
};

/**
 * @brief 解析命令行参数。
 *
 * @param argc main() 传入的参数数量。
 * @param argv main() 传入的参数数组；解析会跳过 argv[0]。
 * @return 参数解析结果。失败时会记录错误类别和触发错误的参数。
 */
ParseResult ParseArgs(int argc, char* argv[]);

}  // 命名空间 termtrans::cli
