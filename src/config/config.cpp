/**
 * @file config.cpp
 * @brief 实现应用配置的 TOML 读写和合并。
 */

#include "config/config.h"

#include <cctype>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <locale>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <config/app_constants.h>
#include <toml++/toml.hpp>

namespace termtrans::config {
namespace {

/**
 * @brief 提取异常中的配置读写错误文本。
 *
 * @param error 捕获到的标准异常。
 * @return 可写入诊断结果的错误文本。
 */
std::string LoadError(const std::exception& error) {
  return error.what();
}

/**
 * @brief 对 ASCII 字符执行小写化。
 *
 * 系统 locale 字符串只用于粗略判断中文环境，不承担完整 BCP 47 解析。
 *
 * @param value 原始环境语言字符串。
 * @return ASCII 小写后的字符串。
 */
std::string LowerAscii(std::string_view value) {
  std::string lowered;
  lowered.reserve(value.size());

  for (const char character : value) {
    lowered.push_back(
        static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
  }

  return lowered;
}

/**
 * @brief 判断系统语言字符串是否表示中文环境。
 *
 * 该函数接受常见的 `zh_CN.UTF-8`、`zh-CN`、`zh` 和包含 Chinese 的
 * locale 名称，避免不同平台 locale 表示差异导致默认语言选择不稳定。
 *
 * @param language 系统 locale 或环境变量值。
 * @return 看起来表示中文环境时返回 true。
 */
bool IsChineseLanguageName(std::string_view language) {
  const std::string lowered = LowerAscii(language);
  return lowered == "zh-cn" || lowered == "zh_cn" || lowered == "zh" ||
         lowered.rfind("zh-", 0) == 0 || lowered.rfind("zh_", 0) == 0 ||
         lowered.find("chinese") != std::string::npos;
}

/**
 * @brief 将非负十进制文本解析为 64 位无符号整数。
 *
 * `max_input_bytes` 的 CLI 取值必须完整消费输入文本，避免 `123abc`
 * 被部分解析成合法值。
 *
 * @param value 原始配置值文本。
 * @return 解析成功时返回数值；空值、负数或溢出时返回空。
 */
std::optional<std::uint64_t> ParseUint64(std::string_view value) {
  if (value.empty()) {
    return std::nullopt;
  }

  std::uint64_t parsed = 0;
  const char* begin = value.data();
  const char* end = begin + value.size();
  const std::from_chars_result result = std::from_chars(begin, end, parsed);
  if (result.ec != std::errc() || result.ptr != end) {
    return std::nullopt;
  }

  return parsed;
}

/**
 * @brief 判断文本是否至少包含一个非空白字符。
 *
 * 模型配置中的 base_url、model_id 和 api_key 都允许 provider 自己定义具体
 * 取值格式，但不能是空白文本。
 *
 * @param value 待检查文本。
 * @return 包含非空白字符时返回 true。
 */
bool HasNonWhitespace(std::string_view value) {
  for (const char character : value) {
    if (std::isspace(static_cast<unsigned char>(character)) == 0) {
      return true;
    }
  }

  return false;
}

/**
 * @brief 判断 provider base URL 是否可用于 HTTP 请求。
 *
 * 第一版只支持 HTTP(S) OpenAI-compatible endpoint。这里只做协议前缀和
 * 非空校验，具体 endpoint 是否可访问由 provider 测试调用确认。
 *
 * @param base_url 用户配置的 provider API 根地址。
 * @return 以 http:// 或 https:// 开头且包含非空白字符时返回 true。
 */
bool IsValidBaseUrl(std::string_view base_url) {
  return HasNonWhitespace(base_url) &&
         (base_url.rfind("https://", 0) == 0 ||
          base_url.rfind("http://", 0) == 0);
}

/**
 * @brief 读取环境变量。
 *
 * @param name 环境变量名，不能为空。
 * @return 变量不存在时返回空字符串。
 */
std::string ReadEnvironmentVariable(const char* name) {
  const char* value = std::getenv(name);
  if (value == nullptr) {
    return "";
  }

  return value;
}

/**
 * @brief 探测当前系统语言名称。
 *
 * 先读取标准 locale 环境变量，避免 `std::locale("")` 在精简运行环境中
 * 抛异常。该函数不保证返回值符合某个严格语言标签格式。
 *
 * @return 系统语言字符串；探测失败时返回空字符串。
 */
std::string DetectSystemLanguageName() {
  for (const char* name : {"LC_ALL", "LC_MESSAGES", "LANG"}) {
    const std::string value = ReadEnvironmentVariable(name);
    if (!value.empty()) {
      return value;
    }
  }

  try {
    return std::locale("").name();
  } catch (const std::runtime_error&) {
    return "";
  }
}

/**
 * @brief 加载用于写回的 TOML 表。
 *
 * 写配置时如果现有配置文件语法已损坏，当前选择从空表重新写入目标
 * 字段，让 `config set ...` 可以修复配置文件，而不是被旧文件永久阻塞。
 *
 * @param path 配置文件路径。
 * @return 可写入的 TOML 表。
 */
toml::table LoadWritableTable(const std::filesystem::path& path) {
  if (!std::filesystem::exists(path)) {
    return toml::table{};
  }

  try {
    return toml::parse_file(path.string());
  } catch (const toml::parse_error&) {
    return toml::table{};
  }
}

/**
 * @brief 从 TOML 顶层读取并校验字符串字段。
 *
 * @param table 已解析的 TOML 表。
 * @param key 顶层字段名。
 * @param target 校验通过时写入的目标字段，不能为空。
 * @param validator 字段值校验函数，不能为空。
 * @return 字段存在且校验通过时返回 true。
 */
bool ReadString(const toml::table& table,
                std::string_view key,
                std::optional<std::string>* target,
                bool (*validator)(std::string_view)) {
  const std::optional<std::string> value =
      table[std::string(key)].value<std::string>();
  if (!value.has_value() || !validator(*value)) {
    return false;
  }

  *target = *value;
  return true;
}

/**
 * @brief 从 TOML 顶层读取布尔字段。
 *
 * @param table 已解析的 TOML 表。
 * @param key 顶层字段名。
 * @param target 字段存在且类型正确时写入的目标字段，不能为空。
 */
void ReadBool(const toml::table& table,
              std::string_view key,
              std::optional<bool>* target) {
  const std::optional<bool> value = table[std::string(key)].value<bool>();
  if (value.has_value()) {
    *target = *value;
  }
}

/**
 * @brief 读取 `max_input_bytes` 配置字段。
 *
 * TOML 整数由 toml++ 暴露为有符号整数；负值表示非法配置并被忽略。
 *
 * @param table 已解析的 TOML 表。
 * @param config 要更新的配置对象，不能为空。
 */
void ReadMaxInputBytes(const toml::table& table, AppConfig* config) {
  const std::optional<std::int64_t> value =
      table["max_input_bytes"].value<std::int64_t>();
  if (value.has_value() && *value >= 0) {
    config->max_input_bytes = static_cast<std::uint64_t>(*value);
  }
}

/**
 * @brief 从 TOML 表读取一个模型配置字段。
 *
 * @param table 模型配置表。
 * @param key 字段名。
 * @return 字段存在且为字符串时返回字段值。
 */
std::optional<std::string> ReadModelString(const toml::table& table,
                                           std::string_view key) {
  return table[std::string(key)].value<std::string>();
}

/**
 * @brief 从 TOML 表读取模型请求超时。
 *
 * 缺失时使用第一版默认值 60 秒；0 表示不设置请求超时上限，非法值会
 * 使当前模型配置被忽略。
 *
 * @param table 模型配置表。
 * @return 合法超时秒数。
 */
std::optional<int> ReadTimeoutSeconds(const toml::table& table) {
  const std::optional<std::int64_t> value =
      table["timeout_seconds"].value<std::int64_t>();
  if (!value.has_value()) {
    return 60;
  }

  if (*value < 0 || *value > std::numeric_limits<int>::max()) {
    return std::nullopt;
  }

  return static_cast<int>(*value);
}

/**
 * @brief 读取配置文件中的模型配置数组。
 *
 * 单个模型配置非法时只忽略该条目；同名模型保留第一个合法条目，避免
 * 后续 `--model <name>` 选择出现歧义。
 *
 * @param table 已解析的 TOML 根表。
 * @param config 要更新的配置对象，不能为空。
 */
void ReadModels(const toml::table& table, AppConfig* config) {
  const toml::array* models_array = table["models"].as_array();
  if (models_array == nullptr) {
    return;
  }

  std::set<std::string> used_names;
  for (const toml::node& node : *models_array) {
    const toml::table* model_table = node.as_table();
    if (model_table == nullptr) {
      continue;
    }

    const std::optional<std::string> name =
        ReadModelString(*model_table, "name");
    const std::optional<std::string> provider_type =
        ReadModelString(*model_table, "provider_type");
    const std::optional<std::string> base_url =
        ReadModelString(*model_table, "base_url");
    const std::optional<std::string> model_id =
        ReadModelString(*model_table, "model_id");
    const std::optional<std::string> api_key =
        ReadModelString(*model_table, "api_key");
    const std::optional<int> timeout_seconds =
        ReadTimeoutSeconds(*model_table);
    if (!name.has_value() || !provider_type.has_value() ||
        !base_url.has_value() || !model_id.has_value() ||
        !api_key.has_value() || !timeout_seconds.has_value()) {
      continue;
    }

    ModelProfile profile{
        .name = *name,
        .provider_type = *provider_type,
        .base_url = *base_url,
        .model_id = *model_id,
        .api_key = *api_key,
        .timeout_seconds = *timeout_seconds,
    };
    if (!IsValidModelProfile(profile) || used_names.contains(profile.name)) {
      continue;
    }

    used_names.insert(profile.name);
    config->models.push_back(std::move(profile));
  }
}

/**
 * @brief 读取配置中的 prompt 覆盖和自定义语言表。
 *
 * 单个 prompt 表非法时只忽略该语言，避免用户手工编辑一个自定义 prompt
 * 失败后影响其它配置字段和其它语言。
 *
 * @param table 已解析的 TOML 表。
 * @param config 要更新的配置对象，不能为空。
 */
void ReadPrompts(const toml::table& table, AppConfig* config) {
  const toml::table* prompts_table = table["prompts"].as_table();
  if (prompts_table == nullptr) {
    return;
  }

  for (const auto& [key, node] : *prompts_table) {
    const std::string_view language = key.str();
    const toml::table* prompt_table = node.as_table();
    if (prompt_table == nullptr || !IsValidTargetLanguage(language)) {
      continue;
    }

    const std::optional<std::string> prompt =
        (*prompt_table)["prompt"].value<std::string>();
    if (prompt.has_value() && IsValidPromptText(*prompt)) {
      config->prompts.emplace(std::string(language), *prompt);
    }
  }
}

/**
 * @brief 构造配置写入失败结果。
 *
 * @param message 稳定的内部错误文本，应用层会映射成本地化用户文案。
 * @return 失败的配置写入结果。
 */
SaveConfigResult SaveError(std::string_view message) {
  return SaveConfigResult{
      .is_ok = false,
      .did_change = false,
      .error_message = std::string(message),
  };
}

/**
 * @brief 将 TOML 表写回配置文件。
 *
 * 所有配置写入入口共享该函数，保证打开失败和写入失败时返回相同的
 * 内部错误文本，应用层可统一映射为本地化诊断。
 *
 * @param path 配置文件路径。
 * @param table 要写入的 TOML 表。
 * @param did_change 当前调用是否实际改变了配置内容。
 * @return 配置写入结果。
 */
SaveConfigResult WriteTable(const std::filesystem::path& path,
                            const toml::table& table,
                            bool did_change) {
  std::ofstream output(path);
  if (!output.is_open()) {
    return SaveConfigResult{
        .is_ok = false,
        .did_change = false,
        .error_message = "failed to open config file",
    };
  }

  output << table << '\n';
  output.close();
  if (!output.good()) {
    return SaveConfigResult{
        .is_ok = false,
        .did_change = false,
        .error_message = "failed to write config file",
    };
  }

  std::error_code permission_error;
  std::filesystem::permissions(
      path, std::filesystem::perms::owner_read |
                std::filesystem::perms::owner_write,
      std::filesystem::perm_options::replace, permission_error);

  return SaveConfigResult{.is_ok = true, .did_change = did_change};
}

/**
 * @brief 校验并写入字符串配置字段。
 *
 * @param table 要更新的 TOML 表，不能为空。
 * @param key TOML 顶层字段名。
 * @param value 原始配置值文本。
 * @param validator 字段值校验函数，不能为空。
 * @return 校验通过并写入时返回 true。
 */
bool SaveStringValue(toml::table* table,
                     std::string_view key,
                     std::string_view value,
                     bool (*validator)(std::string_view)) {
  if (!validator(value)) {
    return false;
  }

  table->insert_or_assign(std::string(key), std::string(value));
  return true;
}

/**
 * @brief 校验并写入 `stream` 配置字段。
 *
 * 当前只接受小写 `true` 和 `false`，避免配置文件里出现大小写混用
 * 或非 TOML 布尔语义。
 *
 * @param table 要更新的 TOML 表，不能为空。
 * @param value 原始配置值文本。
 * @return 校验通过并写入时返回 true。
 */
bool SaveStreamValue(toml::table* table, std::string_view value) {
  if (value == "true") {
    table->insert_or_assign("stream", true);
    return true;
  }

  if (value == "false") {
    table->insert_or_assign("stream", false);
    return true;
  }

  return false;
}

/**
 * @brief 校验并写入 `max_input_bytes` 配置字段。
 *
 * TOML 整数写入必须能无损落在 int64_t 范围内；更大的 uint64_t 值虽然
 * 内存结构可表示，但配置文件无法稳定表达。
 *
 * @param table 要更新的 TOML 表，不能为空。
 * @param value 原始配置值文本。
 * @return 校验通过并写入时返回 true。
 */
bool SaveMaxInputBytesValue(toml::table* table, std::string_view value) {
  const std::optional<std::uint64_t> parsed = ParseUint64(value);
  if (!parsed.has_value() ||
      *parsed > static_cast<std::uint64_t>(
                    std::numeric_limits<std::int64_t>::max())) {
    return false;
  }

  table->insert_or_assign("max_input_bytes",
                          static_cast<std::int64_t>(*parsed));
  return true;
}

/**
 * @brief 获取可写的 `prompts` 表。
 *
 * 如果现有 `prompts` 不是 TOML 表，写入 prompt 时会替换为空表。这与其它
 * config set 行为一致：用户可以通过有效写入修复损坏的局部配置。
 *
 * @param table 要更新的根 TOML 表，不能为空。
 * @return 可写的 `prompts` 表，不能为空。
 */
toml::table* WritablePromptsTable(toml::table* table) {
  toml::table* prompts_table = table->get_as<toml::table>("prompts");
  if (prompts_table != nullptr) {
    return prompts_table;
  }

  table->insert_or_assign("prompts", toml::table{});
  return table->get_as<toml::table>("prompts");
}

/**
 * @brief 将模型配置转换为可写入 TOML 的表。
 *
 * @param profile 已校验的模型配置。
 * @return TOML 模型配置表。
 */
toml::table ModelProfileToTable(const ModelProfile& profile) {
  toml::table model_table;
  model_table.insert_or_assign("name", profile.name);
  model_table.insert_or_assign("provider_type", profile.provider_type);
  model_table.insert_or_assign("base_url", profile.base_url);
  model_table.insert_or_assign("model_id", profile.model_id);
  model_table.insert_or_assign("api_key", profile.api_key);
  model_table.insert_or_assign("timeout_seconds", profile.timeout_seconds);
  return model_table;
}

/**
 * @brief 将模型配置列表转换为 TOML 数组。
 *
 * @param models 当前有效模型配置列表。
 * @return 可写入根表的 TOML 数组。
 */
toml::array ModelsToArray(const std::vector<ModelProfile>& models) {
  toml::array models_array;
  for (const ModelProfile& profile : models) {
    models_array.push_back(ModelProfileToTable(profile));
  }

  return models_array;
}

/**
 * @brief 读取当前配置文件中的有效模型列表。
 *
 * @param path 配置文件路径。
 * @return 读取成功时返回有效模型列表；配置损坏或缺失时返回空列表。
 */
std::vector<ModelProfile> LoadExistingModels(
    const std::filesystem::path& path) {
  const LoadConfigResult load_result = LoadConfig(path);
  if (!load_result.is_ok) {
    return {};
  }

  return load_result.config.models;
}

}  // 匿名命名空间

/**
 * @brief 判断默认模型名是否可作为配置值。
 *
 * 当前配置层尚未读取模型列表，因此这里只排除空名称；是否引用真实模型
 * 配置会在模型管理阶段校验。
 *
 * @param model_name 本地模型配置名称。
 * @return 非空时返回 true。
 */
bool IsValidModelName(std::string_view model_name) {
  return HasNonWhitespace(model_name);
}

/**
 * @brief 判断 provider 类型是否为第一版支持值。
 *
 * @param provider_type provider 类型字符串。
 * @return 仅 openai-compatible 返回 true。
 */
bool IsSupportedProviderType(std::string_view provider_type) {
  return provider_type == "openai-compatible";
}

/**
 * @brief 判断模型配置字段是否完整且可保存。
 *
 * @param profile 待校验的模型配置。
 * @return 所有必填字段合法且 timeout_seconds 为非负整数时返回 true。
 */
bool IsValidModelProfile(const ModelProfile& profile) {
  return IsValidModelName(profile.name) &&
         IsSupportedProviderType(profile.provider_type) &&
         IsValidBaseUrl(profile.base_url) && HasNonWhitespace(profile.model_id) &&
         HasNonWhitespace(profile.api_key) && profile.timeout_seconds >= 0;
}

/**
 * @brief 判断目标语言标识是否可作为配置值。
 *
 * 配置层只校验标识稳定性，避免非 ASCII 展示名直接进入配置键；是否存在
 * 对应 prompt 由 PromptRegistry 校验。
 *
 * @param language 目标语言代码或自定义语言标识。
 * @return 非空、ASCII 且只含字母数字、连字符或下划线时返回 true。
 */
bool IsValidTargetLanguage(std::string_view language) {
  if (language.empty()) {
    return false;
  }

  bool has_alnum = false;
  for (const char character : language) {
    const unsigned char byte = static_cast<unsigned char>(character);
    if (byte > 0x7f) {
      return false;
    }

    if (std::isalnum(byte) != 0) {
      has_alnum = true;
      continue;
    }

    if (character != '-' && character != '_') {
      return false;
    }
  }

  return has_alnum;
}

/**
 * @brief 判断 prompt 文本是否包含有效内容。
 *
 * prompt 允许多行、缩进和非 ASCII 字符，但不能只有空白字符，否则无法
 * 作为后续翻译请求的系统约束。
 *
 * @param prompt prompt 原文。
 * @return 包含至少一个非空白字符时返回 true。
 */
bool IsValidPromptText(std::string_view prompt) {
  for (const char character : prompt) {
    if (std::isspace(static_cast<unsigned char>(character)) == 0) {
      return true;
    }
  }

  return false;
}

/**
 * @brief 判断终端界面语言是否为当前支持值。
 *
 * @param language 界面语言代码。
 * @return zh-CN 或 en 返回 true。
 */
bool IsSupportedUiLanguage(std::string_view language) {
  return language == "zh-CN" || language == "en";
}

/**
 * @brief 判断历史记录模式是否为当前支持值。
 *
 * 配置层只读写和合并该值，不触发历史查询或写入。
 *
 * @param history_mode 历史记录模式。
 * @return ask、reuse、force 或 off 返回 true。
 */
bool IsSupportedHistoryMode(std::string_view history_mode) {
  return history_mode == "ask" || history_mode == "reuse" ||
         history_mode == "force" || history_mode == "off";
}

/**
 * @brief 解析最终目标语言。
 *
 * 配置值合法时优先使用配置；配置缺失时根据系统语言选择 zh-CN 或英文
 * 兜底。
 *
 * @param config 已读取的配置。
 * @return 本次运行使用的目标语言标识。
 */
std::string ResolveTargetLanguage(const AppConfig& config) {
  if (config.target_language.has_value() &&
      IsValidTargetLanguage(*config.target_language)) {
    return *config.target_language;
  }

  if (IsChineseLanguageName(DetectSystemLanguageName())) {
    return "zh-CN";
  }

  return termtrans::config::constants::app::kDefaultTargetLanguage;
}

/**
 * @brief 解析最终终端界面语言。
 *
 * 配置值合法时优先使用配置；配置缺失时根据系统语言选择 zh-CN 或英文
 * 兜底。
 *
 * @param config 已读取的配置。
 * @return 本次运行使用的界面语言。
 */
std::string ResolveUiLanguage(const AppConfig& config) {
  if (config.ui_language.has_value() &&
      IsSupportedUiLanguage(*config.ui_language)) {
    return *config.ui_language;
  }

  if (IsChineseLanguageName(DetectSystemLanguageName())) {
    return "zh-CN";
  }

  return termtrans::config::constants::app::kDefaultUiLanguage;
}

/**
 * @brief 合并配置文件、CLI 覆盖和内置默认值。
 *
 * 合并结果只表达配置层能确定的运行配置；模型是否存在、目标语言是否
 * 有 prompt 等跨模块约束由对应模块处理。
 *
 * @param config 已读取的配置文件字段。
 * @param overrides 当前 CLI 调用提供的覆盖项。
 * @return 后续应用流程使用的确定配置。
 */
ResolvedConfig ResolveConfig(const AppConfig& config,
                             const ConfigOverrides& overrides) {
  ResolvedConfig resolved;

  if (config.default_model.has_value() &&
      IsValidModelName(*config.default_model) &&
      FindModelProfile(config, *config.default_model) != nullptr) {
    resolved.default_model = *config.default_model;
  } else if (!config.models.empty()) {
    resolved.default_model = config.models.front().name;
  }

  resolved.target_language = ResolveTargetLanguage(config);
  if (overrides.target_language.has_value() &&
      IsValidTargetLanguage(*overrides.target_language)) {
    resolved.target_language = *overrides.target_language;
  }

  if (overrides.model_name.has_value() &&
      IsValidModelName(*overrides.model_name)) {
    resolved.default_model = *overrides.model_name;
  }

  resolved.ui_language = ResolveUiLanguage(config);
  resolved.stream =
      config.stream.value_or(termtrans::config::constants::app::kDefaultStream);
  if (overrides.stream.has_value()) {
    resolved.stream = *overrides.stream;
  }

  resolved.history_mode =
      config.history_mode.value_or(
          termtrans::config::constants::app::kDefaultHistoryMode);
  if (overrides.history_mode.has_value() &&
      IsSupportedHistoryMode(*overrides.history_mode)) {
    resolved.history_mode = *overrides.history_mode;
  }

  resolved.max_input_bytes = config.max_input_bytes.value_or(
      termtrans::config::constants::app::kDefaultMaxInputBytes);

  return resolved;
}

/**
 * @brief 在模型列表中查找指定本地模型配置。
 *
 * @param config 已读取的配置。
 * @param model_name 本地模型配置名称。
 * @return 找到时返回模型配置指针；否则返回 nullptr。
 */
const ModelProfile* FindModelProfile(const AppConfig& config,
                                     std::string_view model_name) {
  for (const ModelProfile& profile : config.models) {
    if (profile.name == model_name) {
      return &profile;
    }
  }

  return nullptr;
}

/**
 * @brief 从 TOML 文件读取当前支持的配置字段。
 *
 * 文件缺失不是错误；单个字段值非法会被忽略，TOML 语法错误才会使读取
 * 失败。
 *
 * @param path 配置文件路径。
 * @return 配置读取结果。
 */
LoadConfigResult LoadConfig(const std::filesystem::path& path) {
  if (!std::filesystem::exists(path)) {
    return LoadConfigResult{.is_ok = true};
  }

  try {
    const toml::table table = toml::parse_file(path.string());
    AppConfig config;

    ReadString(table, "default_model", &config.default_model, IsValidModelName);
    ReadString(table, "target_language", &config.target_language,
               IsValidTargetLanguage);
    ReadString(table, "ui_language", &config.ui_language,
               IsSupportedUiLanguage);
    ReadBool(table, "stream", &config.stream);
    ReadString(table, "history_mode", &config.history_mode,
               IsSupportedHistoryMode);
    ReadMaxInputBytes(table, &config);
    ReadModels(table, &config);
    ReadPrompts(table, &config);

    return LoadConfigResult{.is_ok = true, .config = config};
  } catch (const std::exception& error) {
    return LoadConfigResult{
        .is_ok = false,
        .error_message = LoadError(error),
    };
  }
}

/**
 * @brief 保存或替换一个模型配置。
 *
 * 同名模型会被新配置替换；第一个模型或显式要求默认时，会同步设置
 * default_model。
 *
 * @param path 配置文件路径。
 * @param profile 要保存的模型配置。
 * @param set_default 是否将该模型设为默认模型。
 * @return 写入结果。
 */
SaveConfigResult SaveModelProfile(const std::filesystem::path& path,
                                  const ModelProfile& profile,
                                  bool set_default) {
  if (!IsValidModelProfile(profile)) {
    return SaveError("invalid model profile");
  }

  try {
    std::filesystem::create_directories(path.parent_path());

    toml::table table = LoadWritableTable(path);
    std::vector<ModelProfile> models = LoadExistingModels(path);
    const bool is_first_model = models.empty();
    bool did_replace = false;
    for (ModelProfile& existing_profile : models) {
      if (existing_profile.name == profile.name) {
        existing_profile = profile;
        did_replace = true;
        break;
      }
    }

    if (!did_replace) {
      models.push_back(profile);
    }

    table.insert_or_assign("models", ModelsToArray(models));
    if (set_default || is_first_model) {
      table.insert_or_assign("default_model", profile.name);
    }

    return WriteTable(path, table, true);
  } catch (const std::exception& error) {
    return SaveConfigResult{
        .is_ok = false,
        .did_change = false,
        .error_message = LoadError(error),
    };
  }
}

/**
 * @brief 将已有模型设置为默认模型。
 *
 * @param path 配置文件路径。
 * @param model_name 已存在的模型配置名称。
 * @return 写入结果。
 */
SaveConfigResult SetDefaultModel(const std::filesystem::path& path,
                                 std::string_view model_name) {
  if (!IsValidModelName(model_name)) {
    return SaveError("invalid model name");
  }

  const LoadConfigResult load_result = LoadConfig(path);
  if (!load_result.is_ok ||
      FindModelProfile(load_result.config, model_name) == nullptr) {
    return SaveError("model not found");
  }

  try {
    std::filesystem::create_directories(path.parent_path());

    toml::table table = LoadWritableTable(path);
    table.insert_or_assign("default_model", std::string(model_name));
    return WriteTable(path, table, true);
  } catch (const std::exception& error) {
    return SaveConfigResult{
        .is_ok = false,
        .did_change = false,
        .error_message = LoadError(error),
    };
  }
}

/**
 * @brief 写入一个配置字段并保留可解析的其它字段。
 *
 * 写入时采用严格校验：非法键或非法值会失败。现有配置文件若语法
 * 损坏，会从空表重写目标字段，以便用户通过 `config set` 修复配置。
 *
 * @param path 配置文件路径。
 * @param key CLI 使用的配置键名。
 * @param value 原始配置值文本。
 * @return 配置写入结果。
 */
SaveConfigResult SaveConfigValue(const std::filesystem::path& path,
                                 std::string_view key,
                                 std::string_view value) {
  try {
    std::filesystem::create_directories(path.parent_path());

    toml::table table = LoadWritableTable(path);
    bool is_valid = false;

    if (key == "target-language") {
      is_valid = SaveStringValue(&table, "target_language", value,
                                 IsValidTargetLanguage);
    } else if (key == "ui-language") {
      is_valid =
          SaveStringValue(&table, "ui_language", value, IsSupportedUiLanguage);
    } else if (key == "stream") {
      is_valid = SaveStreamValue(&table, value);
    } else if (key == "history-mode") {
      is_valid =
          SaveStringValue(&table, "history_mode", value, IsSupportedHistoryMode);
    } else if (key == "max-input-bytes") {
      is_valid = SaveMaxInputBytesValue(&table, value);
    } else {
      return SaveError("unsupported config key");
    }

    if (!is_valid) {
      return SaveError("invalid config value");
    }

    return WriteTable(path, table, true);
  } catch (const std::exception& error) {
    return SaveConfigResult{
        .is_ok = false,
        .did_change = false,
        .error_message = LoadError(error),
    };
  }
}

/**
 * @brief 写入一个目标语言的完整 prompt。
 *
 * 该函数只负责配置持久化，不判断目标语言是否为内置语言；内置覆盖和
 * 自定义语言新增由 PromptRegistry 在读取配置后统一合并。
 *
 * @param path 配置文件路径。
 * @param language 目标语言代码或自定义语言标识。
 * @param prompt_text 完整 prompt 文本。
 * @return 写入结果。
 */
SaveConfigResult SavePrompt(const std::filesystem::path& path,
                            std::string_view language,
                            std::string_view prompt_text) {
  if (!IsValidTargetLanguage(language)) {
    return SaveError("invalid language");
  }

  if (!IsValidPromptText(prompt_text)) {
    return SaveError("invalid prompt");
  }

  try {
    std::filesystem::create_directories(path.parent_path());

    toml::table table = LoadWritableTable(path);
    toml::table* prompts_table = WritablePromptsTable(&table);
    if (prompts_table == nullptr) {
      return SaveError("failed to create prompts table");
    }

    toml::table prompt_table;
    prompt_table.insert_or_assign("prompt", std::string(prompt_text));
    prompts_table->insert_or_assign(std::string(language),
                                    std::move(prompt_table));

    return WriteTable(path, table, true);
  } catch (const std::exception& error) {
    return SaveConfigResult{
        .is_ok = false,
        .did_change = false,
        .error_message = LoadError(error),
    };
  }
}

/**
 * @brief 删除目标语言的配置 prompt。
 *
 * 缺失配置项时返回成功且 did_change 为 false，使 `languages unset` 成为
 * 幂等命令；这不会影响内置 prompt 的可用性。
 *
 * @param path 配置文件路径。
 * @param language 目标语言代码或自定义语言标识。
 * @return 删除结果。
 */
SaveConfigResult UnsetPrompt(const std::filesystem::path& path,
                             std::string_view language) {
  if (!IsValidTargetLanguage(language)) {
    return SaveError("invalid language");
  }

  try {
    std::filesystem::create_directories(path.parent_path());

    toml::table table = LoadWritableTable(path);
    toml::table* prompts_table = table.get_as<toml::table>("prompts");
    if (prompts_table == nullptr || !prompts_table->contains(language)) {
      return SaveConfigResult{.is_ok = true, .did_change = false};
    }

    prompts_table->erase(language);
    if (prompts_table->empty()) {
      table.erase("prompts");
    }

    return WriteTable(path, table, true);
  } catch (const std::exception& error) {
    return SaveConfigResult{
        .is_ok = false,
        .did_change = false,
        .error_message = LoadError(error),
    };
  }
}

}  // 命名空间 termtrans::config
