/**
 * @file plain_writer.h
 * @brief 声明 stdout 纯译文输出器。
 */

#pragma once

#include "translate/translator.h"

#include <iosfwd>
#include <string>
#include <string_view>

namespace termtrans::output {

/**
 * @brief 将译文写入 stdout 的输出器。
 *
 * PlainWriter 不输出进度、状态或诊断信息，确保 stdout 永远只包含译文。
 * 写入失败通常表示下游管道关闭，此时会标记取消并让调用方停止后续
 * provider 请求。
 *
 * @note 该类不是线程安全的。
 */
class PlainWriter : public translate::TranslationStreamSink {
 public:
  /**
   * @brief 创建 stdout 纯译文输出器。
   *
   * @param output_stream 译文输出流，通常为 std::cout。
   */
  explicit PlainWriter(std::ostream& output_stream);

  /**
   * @brief 写入一段译文。
   *
   * @param text 译文增量或完整译文。
   * @return 写入成功时返回 true；失败时标记取消并返回 false。
   */
  bool Write(std::string_view text) override;

  /**
   * @brief 返回下游是否已经取消。
   *
   * @return stdout 写入失败后返回 true。
   */
  bool IsCancelled() const;

 private:
  std::ostream& output_stream_;  // 不拥有；由应用入口持有。
  bool is_cancelled_ = false;  // 下游写入失败标记。
};

}  // namespace termtrans::output
