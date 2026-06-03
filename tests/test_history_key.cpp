/**
 * @file test_history_key.cpp
 * @brief 测试翻译历史 key 计算。
 */

#include "history/history_key.h"

#include <iostream>

namespace {

/**
 * @brief 记录单个测试断言失败信息。
 */
bool Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    return false;
  }

  return true;
}

/**
 * @brief 验证 SHA-256 摘要稳定且使用完整输入字节。
 */
int ShouldComputeStableSha256() {
  const termtrans::history::HistoryKey key =
      termtrans::history::ComputeHistoryKey("hello", "zh-CN");

  return Expect(key.input_sha256 ==
                    "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e730"
                    "43362938b9824",
                "hello 的 SHA-256 应稳定") &&
                 Expect(key.target_language == "zh-CN",
                        "历史 key 应保存目标语言")
             ? 0
             : 1;
}

/**
 * @brief 验证模型等元数据不会参与 key 计算。
 */
int ShouldOnlyDependOnInputAndTargetLanguage() {
  const termtrans::history::HistoryKey first =
      termtrans::history::ComputeHistoryKey("hello", "zh-CN");
  const termtrans::history::HistoryKey second =
      termtrans::history::ComputeHistoryKey("hello", "zh-CN");
  const termtrans::history::HistoryKey different_input =
      termtrans::history::ComputeHistoryKey("hello!", "zh-CN");
  const termtrans::history::HistoryKey different_language =
      termtrans::history::ComputeHistoryKey("hello", "en");

  return Expect(first.input_sha256 == second.input_sha256,
                "相同输入应得到相同摘要") &&
                 Expect(first.input_sha256 != different_input.input_sha256,
                        "不同输入应得到不同摘要") &&
                 Expect(first.input_sha256 == different_language.input_sha256,
                        "目标语言不应影响输入摘要") &&
                 Expect(first.target_language !=
                            different_language.target_language,
                        "目标语言应作为 key 的独立字段保存")
             ? 0
             : 1;
}

}  // 匿名命名空间

/**
 * @brief 运行历史 key 测试。
 */
int main() {
  if (ShouldComputeStableSha256() != 0) {
    return 1;
  }
  if (ShouldOnlyDependOnInputAndTargetLanguage() != 0) {
    return 1;
  }

  return 0;
}
