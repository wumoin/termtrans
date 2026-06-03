/**
 * @file history_record.h
 * @brief 声明翻译历史记录的数据结构。
 */

#pragma once

#include "history/history_key.h"

#include <cstdint>
#include <string>

namespace termtrans::history {

/**
 * @brief 一条完整翻译历史记录。
 *
 * 记录保存原文、译文和用于排查来源的模型元数据。API key 属于敏感配置，
 * 不进入历史记录，也不应出现在 history list 输出中。
 *
 * @note 该结构不包含同步机制，调用方负责跨线程访问保护。
 */
struct HistoryRecord {
  HistoryKey key;  // 历史命中 key，只由输入摘要和目标语言组成。
  std::string input_text;  // 完整输入文本，用于后续审计或调试。
  std::string translated_text;  // 完整译文，命中历史时直接输出。
  std::string model_name;  // 本地模型配置名称。
  std::string provider_type;  // provider 类型，例如 openai-compatible。
  std::string model_id;  // provider 侧真实模型 ID。
  std::string base_url;  // provider API 根地址，不含 API key。
  std::string created_at;  // 首次保存时间，UTC ISO-8601 文本。
  std::string updated_at;  // 最近更新保存时间，UTC ISO-8601 文本。
  std::uint64_t input_bytes = 0;  // 原文大小，单位字节。
  std::uint64_t output_bytes = 0;  // 译文大小，单位字节。
};

}  // namespace termtrans::history
