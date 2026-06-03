/**
 * @file history_key.h
 * @brief 声明翻译历史命中 key 的计算接口。
 */

#pragma once

#include <string>
#include <string_view>

namespace termtrans::history {

/**
 * @brief 翻译历史命中使用的稳定 key。
 *
 * HistoryKey 只包含输入文本的 SHA-256 和目标语言。模型名称、provider、
 * prompt 和其它运行参数只作为历史元数据保存，不参与命中判断。
 *
 * @note 该结构不包含同步机制，调用方负责跨线程访问保护。
 */
struct HistoryKey {
  std::string input_sha256;  // 完整输入文本的 SHA-256 十六进制摘要。
  std::string target_language;  // 本次翻译的目标语言标识。
};

/**
 * @brief 计算完整输入文本对应的历史 key。
 *
 * @param input_text 完整输入文本，按原始字节计算 SHA-256。
 * @param target_language 本次翻译目标语言。
 * @return 仅由 input_text 和 target_language 构成的历史 key。
 */
HistoryKey ComputeHistoryKey(std::string_view input_text,
                             std::string_view target_language);

}  // namespace termtrans::history
