/**
 * @file progress_indicator.h
 * @brief 声明非流式翻译进度提示。
 */

#pragma once

#include <iosfwd>
#include <string_view>

namespace termtrans::output {

/**
 * @brief 非流式翻译时写入 stderr 的简易进度提示。
 *
 * ProgressIndicator 只在调用方确认 stderr 是交互终端时输出，且永不写入
 * stdout，避免污染管道译文。
 *
 * @note 该类不是线程安全的。
 */
class ProgressIndicator {
 public:
  /**
   * @brief 创建进度提示器。
   *
   * @param stderr_stream 诊断输出流。
   * @param is_enabled 是否启用输出。
   */
  ProgressIndicator(std::ostream& stderr_stream, bool is_enabled);

  /**
   * @brief 输出开始提示。
   *
   * @param message 已本地化的进度文本。
   */
  void Start(std::string_view message);

  /**
   * @brief 清理进度提示状态。
   */
  void Finish();

 private:
  std::ostream& stderr_stream_;  // 不拥有；由应用入口持有。
  bool is_enabled_ = false;  // 是否实际输出进度。
  bool did_start_ = false;  // 是否已经输出开始提示。
};

}  // namespace termtrans::output
