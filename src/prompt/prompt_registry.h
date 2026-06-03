/**
 * @file prompt_registry.h
 * @brief 声明目标语言 prompt 注册表。
 */

#pragma once

#include "config/config.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace termtrans::prompt {

/**
 * @brief 当前生效 prompt 的来源。
 */
enum class PromptSource {
  kBuiltin,  // 来自程序内置目标语言表。
  kConfig,   // 来自配置文件 prompts 表。
};

/**
 * @brief 一个可用目标语言的生效 prompt 条目。
 *
 * 条目只描述当前可用于翻译请求的完整 prompt。它不保存 prompt id 或
 * prompt version，也不负责把输入 chunk 或短期上下文拼接进 provider 请求。
 */
struct PromptEntry {
  std::string code;  // 目标语言代码或自定义语言标识。
  std::string display_name;  // 用户可见名称；自定义语言默认等于 code。
  std::string prompt;  // 当前生效的完整 prompt 文本。
  PromptSource source = PromptSource::kBuiltin;  // 当前 prompt 的来源。
  bool is_builtin = false;  // 该语言代码是否存在内置定义。
};

/**
 * @brief 合并内置目标语言和配置 prompt 的注册表。
 *
 * PromptRegistry 负责解析“内置语言表 + 配置 prompts 表”的当前可用语言
 * 集合。配置 prompt 会覆盖同名内置语言；非内置 code 会成为自定义目标
 * 语言。该类不读取或写入配置文件，不调用 provider，也不管理历史记录。
 *
 * @note 该类构造后不可变，可并发只读访问。
 */
class PromptRegistry {
 public:
  /**
   * @brief 使用已读取配置构造 prompt 注册表。
   *
   * @param config 已由 config::LoadConfig() 过滤过的有效配置字段。
   */
  explicit PromptRegistry(const config::AppConfig& config);

  /**
   * @brief 返回当前可用目标语言列表。
   *
   * @return 按 language code 稳定排序后的生效 prompt 条目。
   */
  std::vector<PromptEntry> ListLanguages() const;

  /**
   * @brief 查找目标语言当前生效的完整 prompt。
   *
   * @param language 目标语言代码或自定义语言标识。
   * @return 语言可用时返回生效条目，否则返回空。
   */
  std::optional<PromptEntry> Resolve(std::string_view language) const;

  /**
   * @brief 判断语言 code 是否存在内置定义。
   *
   * @param language 目标语言代码。
   * @return 内置目标语言表包含该 code 时返回 true。
   */
  bool HasBuiltinLanguage(std::string_view language) const;

 private:
  std::vector<PromptEntry> entries_;  // 当前配置下的生效语言列表。
};

/**
 * @brief 将 prompt 来源转换为稳定英文标识。
 *
 * @param source prompt 来源。
 * @return `builtin` 或 `config`。
 */
std::string_view PromptSourceName(PromptSource source);

}  // namespace termtrans::prompt
