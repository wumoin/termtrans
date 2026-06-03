/**
 * @file config.h
 * @brief 声明应用配置的读取、写入和合并接口。
 */

#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace termtrans::config {

/**
 * @brief 一个本地命名的 provider 模型配置。
 *
 * 模型配置是用户日常通过 `--model <name>` 选择的最小单位。它保存调用
 * OpenAI-compatible provider 所需的本地参数，但不负责发起网络请求，也不
 * 负责在用户可见输出中隐藏 api_key；调用方输出时必须主动避免泄露。
 *
 * @note 该结构不包含同步机制，调用方负责跨线程访问保护。
 */
struct ModelProfile {
  std::string name;  // 本地唯一模型配置名称。
  std::string provider_type;  // 第一版仅支持 openai-compatible。
  std::string base_url;  // provider API 根地址，不含具体 endpoint 也可工作。
  std::string model_id;  // provider 侧真实模型 ID。
  std::string api_key;  // provider API key；不得写入日志或列表输出。
  // 单次 provider 请求超时时间，单位秒；0 表示不设置请求超时上限。
  int timeout_seconds = 60;
};

/**
 * @brief 配置文件中当前已支持的应用配置。
 *
 * 该结构只承载 TOML 中读取到的有效字段。无效字段会被忽略，避免一个
 * 局部配置拼写错误导致整个配置文件不可读取。它不负责保存配置文件
 * 路径，也不表达内置默认值；缺失字段由 ResolveConfig()、PromptRegistry
 * 和对应解析函数兜底。
 *
 * @note 该结构不包含同步机制，调用方负责跨线程访问保护。
 */
struct AppConfig {
  // 默认本地模型配置名称；模型阶段再校验模型是否存在。
  std::optional<std::string> default_model;
  // 默认目标语言；PromptRegistry 再校验是否有可用 prompt。
  std::optional<std::string> target_language;
  std::optional<std::string> ui_language;  // 终端界面语言，仅支持 zh-CN 和 en。
  std::optional<bool> stream;  // 默认是否启用流式输出。
  std::optional<std::string> history_mode;  // 默认历史策略：ask、reuse、force 或 off。
  // 单个翻译分段的目标最大字节数，0 表示不按用户上限分段。
  std::optional<std::uint64_t> max_input_bytes;
  // 配置文件中的模型配置列表，按 TOML 中出现顺序保留。
  std::vector<ModelProfile> models;
  // 配置文件中的完整 prompt 覆盖或自定义语言，key 为目标语言标识。
  std::map<std::string, std::string> prompts;
};

/**
 * @brief 命令行参数对配置文件的临时覆盖。
 *
 * 该结构只表示当前进程调用的覆盖值，不会被写回配置文件。
 * 它不负责解析 CLI，也不校验模型或目标语言是否存在。
 *
 * @note 该结构不包含同步机制，调用方负责跨线程访问保护。
 */
struct ConfigOverrides {
  std::optional<std::string> target_language;  // CLI `--to` 覆盖值。
  std::optional<std::string> model_name;  // CLI `--model` 覆盖值。
  std::optional<bool> stream;  // CLI `--stream` / `--no-stream` 覆盖值。
  std::optional<std::string> history_mode;  // CLI 历史 flag 覆盖值。
};

/**
 * @brief 经过“CLI 参数 > 配置文件 > 内置默认值”合并后的配置。
 *
 * 该结构表示当前进程调用已经确定的运行配置。它不保证模型、目标语言
 * prompt 或历史记录资源已经存在；这些跨模块约束由对应模块负责校验。
 *
 * @note 该结构不包含同步机制，调用方负责跨线程访问保护。
 */
struct ResolvedConfig {
  // 解析后的默认模型名；空字符串表示尚未配置模型。
  std::string default_model;
  std::string target_language;  // 解析后的目标语言。
  std::string ui_language;  // 解析后的终端界面语言。
  bool stream = true;  // 解析后的流式输出开关。
  std::string history_mode;  // 解析后的历史策略。
  std::uint64_t max_input_bytes = 0;  // 解析后的翻译分段上限，单位字节。
};

/**
 * @brief 配置读取结果。
 *
 * 该结构承载 LoadConfig() 的返回状态和有效配置字段，不负责输出诊断或做
 * 本地化转换。
 */
struct LoadConfigResult {
  bool is_ok = false;  // TOML 文件是否成功读取或不存在。
  AppConfig config;  // 读取成功时得到的有效配置字段集合。
  std::string error_message;  // 读取失败时的底层错误文本。
};

/**
 * @brief 配置写入结果。
 *
 * 该结构承载 SaveConfigValue() 的内部错误文本。用户可见错误由应用层映射
 * 为本地化文案。
 */
struct SaveConfigResult {
  bool is_ok = false;  // 配置文件是否成功写入。
  // 写入请求是否实际改变了配置文件；unset 缺少配置项时为 false。
  bool did_change = false;
  std::string error_message;  // 写入失败或配置值非法时的错误文本。
};

/**
 * @brief 判断默认模型名是否为当前阶段可接受的配置值。
 *
 * @param model_name 本地模型配置名称。
 * @return 非空值返回 true；模型表存在性会在后续模型阶段校验。
 */
bool IsValidModelName(std::string_view model_name);

/**
 * @brief 判断 provider 类型是否为第一版支持值。
 *
 * @param provider_type provider 类型字符串。
 * @return 仅 openai-compatible 返回 true。
 */
bool IsSupportedProviderType(std::string_view provider_type);

/**
 * @brief 判断模型配置字段是否完整且可保存。
 *
 * @param profile 待校验的模型配置。
 * @return 所有必填字段合法且 timeout_seconds 为非负整数时返回 true。
 */
bool IsValidModelProfile(const ModelProfile& profile);

/**
 * @brief 判断目标语言标识是否为当前阶段可接受的配置值。
 *
 * @param language 目标语言代码或自定义语言标识。
 * @return 非空 ASCII 标识返回 true；是否存在于 prompt 表由 PromptRegistry 校验。
 */
bool IsValidTargetLanguage(std::string_view language);

/**
 * @brief 判断 prompt 文本是否可作为完整配置 prompt 保存。
 *
 * @param prompt prompt 原文。
 * @return 至少包含一个非空白字符时返回 true。
 */
bool IsValidPromptText(std::string_view prompt);

/**
 * @brief 判断界面语言是否为当前支持的配置值。
 *
 * @param language 界面语言代码。
 * @return 仅 zh-CN 和 en 返回 true。
 */
bool IsSupportedUiLanguage(std::string_view language);

/**
 * @brief 判断历史记录模式是否为当前支持的配置值。
 *
 * @param history_mode 历史记录模式。
 * @return ask、reuse、force 和 off 返回 true。
 */
bool IsSupportedHistoryMode(std::string_view history_mode);

/**
 * @brief 按“配置优先、系统语言兜底”的规则解析目标语言。
 *
 * @param config 已读取的应用配置。
 * @return 配置值、系统目标语言或内置英文兜底值。
 */
std::string ResolveTargetLanguage(const AppConfig& config);

/**
 * @brief 按“配置优先、系统语言兜底”的规则解析界面语言。
 *
 * @param config 已读取的应用配置。
 * @return zh-CN 或 en。
 */
std::string ResolveUiLanguage(const AppConfig& config);

/**
 * @brief 合并配置文件、命令行覆盖和内置默认值。
 *
 * @param config 已读取的应用配置。
 * @param overrides 当前 CLI 调用提供的覆盖值。
 * @return 后续应用流程使用的确定配置。
 */
ResolvedConfig ResolveConfig(const AppConfig& config,
                             const ConfigOverrides& overrides);

/**
 * @brief 在模型列表中查找指定本地模型配置。
 *
 * @param config 已读取的配置。
 * @param model_name 本地模型配置名称。
 * @return 找到时返回模型配置指针；否则返回 nullptr。
 */
const ModelProfile* FindModelProfile(const AppConfig& config,
                                     std::string_view model_name);

/**
 * @brief 从 TOML 配置文件读取当前支持的配置。
 *
 * @param path 配置文件路径。
 * @return 配置读取结果。文件不存在时返回成功和空配置；字段值非法时
 *     忽略该字段。
 */
LoadConfigResult LoadConfig(const std::filesystem::path& path);

/**
 * @brief 写入单个通用配置字段并保留文件中的其它字段。
 *
 * @param path 配置文件路径。
 * @param key CLI 使用的配置键名。
 * @param value 配置值文本。
 * @return 写入结果。必要时会创建父目录；非法键或非法值会返回失败。
 */
SaveConfigResult SaveConfigValue(const std::filesystem::path& path,
                                 std::string_view key,
                                 std::string_view value);

/**
 * @brief 保存或替换一个模型配置。
 *
 * 该函数会保留其它配置字段和其它模型配置；同名模型会被新配置替换。
 * 当 set_default 为 true，或配置文件此前没有任何模型时，会同步更新
 * default_model。
 *
 * @param path 配置文件路径。
 * @param profile 要保存的模型配置。
 * @param set_default 是否将该模型设为默认模型。
 * @return 写入结果。非法模型字段会返回失败。
 */
SaveConfigResult SaveModelProfile(const std::filesystem::path& path,
                                  const ModelProfile& profile,
                                  bool set_default);

/**
 * @brief 将已有模型设置为默认模型。
 *
 * @param path 配置文件路径。
 * @param model_name 已存在的本地模型配置名称。
 * @return 写入结果。模型不存在时返回失败。
 */
SaveConfigResult SetDefaultModel(const std::filesystem::path& path,
                                 std::string_view model_name);

/**
 * @brief 写入目标语言的完整 prompt 配置。
 *
 * @param path 配置文件路径。
 * @param language 目标语言代码或自定义语言标识。
 * @param prompt_text 从用户文件读取到的完整 prompt 文本。
 * @return 写入结果。必要时会创建父目录；非法语言标识或空 prompt 会失败。
 */
SaveConfigResult SavePrompt(const std::filesystem::path& path,
                            std::string_view language,
                            std::string_view prompt_text);

/**
 * @brief 删除目标语言的配置 prompt。
 *
 * 该函数只删除 `[prompts."<language>"]` 配置项，不删除内置语言定义。
 * 缺少对应配置项时仍返回成功，但 did_change 为 false，供应用层输出幂等
 * 提示。
 *
 * @param path 配置文件路径。
 * @param language 目标语言代码或自定义语言标识。
 * @return 删除结果。非法语言标识会失败。
 */
SaveConfigResult UnsetPrompt(const std::filesystem::path& path,
                             std::string_view language);

}  // 命名空间 termtrans::config
