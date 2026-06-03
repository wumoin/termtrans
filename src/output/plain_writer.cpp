/**
 * @file plain_writer.cpp
 * @brief 实现 stdout 纯译文输出器。
 */

#include "output/plain_writer.h"

#include <ostream>
#include <string_view>

namespace termtrans::output {

/**
 * @brief 创建 stdout 纯译文输出器。
 */
PlainWriter::PlainWriter(std::ostream& output_stream)
    : output_stream_(output_stream) {}

/**
 * @brief 写入一段译文。
 */
bool PlainWriter::Write(std::string_view text) {
  if (is_cancelled_) {
    return false;
  }

  output_stream_ << text << std::flush;
  if (!output_stream_.good()) {
    is_cancelled_ = true;
    return false;
  }

  return true;
}

/**
 * @brief 返回下游是否已经取消。
 */
bool PlainWriter::IsCancelled() const { return is_cancelled_; }

}  // namespace termtrans::output
