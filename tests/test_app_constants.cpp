/**
 * @file test_app_constants.cpp
 * @brief 测试手写编译期应用常量。
 */

#include "config/app_constants.h"

#include <iostream>
#include <string_view>

namespace {

/**
 * @brief 记录单个测试断言失败信息。
 *
 * @param condition 断言条件。
 * @param message 断言失败时写入 stderr 的说明。
 * @return 条件为 true 时返回 true。
 */
bool Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    return false;
  }

  return true;
}

}  // 匿名命名空间

/**
 * @brief 验证手写编译期默认值符合阶段 2 设计。
 *
 * @return 任一常量不符合预期时返回 1。
 */
int main() {
  namespace app_constants = termtrans::config::constants::app;

  const bool is_valid =
      Expect(std::string_view(app_constants::kApplicationName) == "termtrans",
             "application name should be termtrans") &&
      Expect(std::string_view(app_constants::kDefaultTargetLanguage) == "en",
             "default target language should be en") &&
      Expect(std::string_view(app_constants::kDefaultUiLanguage) == "en",
             "default UI language should be en") &&
      Expect(app_constants::kDefaultStream,
             "default stream should be enabled") &&
      Expect(std::string_view(app_constants::kDefaultHistoryMode) == "ask",
             "default history mode should be ask") &&
      Expect(app_constants::kDefaultMaxInputBytes == 0,
             "default max input bytes should be zero");

  return is_valid ? 0 : 1;
}
