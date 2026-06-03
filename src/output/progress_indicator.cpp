/**
 * @file progress_indicator.cpp
 * @brief 实现非流式翻译进度提示。
 */

#include "output/progress_indicator.h"

#include <ostream>
#include <string_view>

namespace termtrans::output {

/**
 * @brief 创建进度提示器。
 */
ProgressIndicator::ProgressIndicator(std::ostream& stderr_stream,
                                     bool is_enabled)
    : stderr_stream_(stderr_stream), is_enabled_(is_enabled) {}

/**
 * @brief 输出开始提示。
 */
void ProgressIndicator::Start(std::string_view message) {
  if (!is_enabled_ || did_start_) {
    return;
  }

  stderr_stream_ << message << '\n';
  did_start_ = true;
}

/**
 * @brief 清理进度提示状态。
 */
void ProgressIndicator::Finish() {
  if (!is_enabled_ || !did_start_) {
    return;
  }

  stderr_stream_.flush();
}

}  // namespace termtrans::output
