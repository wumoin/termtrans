/**
 * @file chunker.h
 * @brief 声明翻译输入分块器。
 */

#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace termtrans::translate {

/**
 * @brief 将输入文本切分为 provider 请求 chunk。
 *
 * Chunker 只处理文本边界，不理解翻译业务、不访问 provider，也不决定
 * 输出策略。0 表示不按用户字节上限分块。
 *
 * @note 该类不保存共享可变状态，可并发使用不同实例。
 */
class Chunker {
 public:
  /**
   * @brief 创建分块器。
   *
   * @param max_chunk_bytes 单个 chunk 的目标最大字节数，0 表示不限制。
   */
  explicit Chunker(std::size_t max_chunk_bytes = 6000);

  /**
   * @brief 按稳定边界切分输入文本。
   *
   * @param text 完整输入文本。
   * @return 非空 chunk 列表；全空白输入返回空列表。
   */
  std::vector<std::string> Split(std::string_view text) const;

 private:
  std::size_t max_chunk_bytes_ = 6000;  // 单 chunk 目标上限；0 表示不限制。
};

}  // namespace termtrans::translate
