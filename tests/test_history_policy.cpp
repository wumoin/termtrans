/**
 * @file test_history_policy.cpp
 * @brief 测试历史命中策略判断。
 */

#include "history/history_policy.h"

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
 * @brief 验证不同历史模式的查询和保存开关。
 */
int ShouldSelectLookupAndSaveBehavior() {
  return Expect(termtrans::history::ShouldLookupHistory("ask"),
                "ask 应查询历史") &&
                 Expect(termtrans::history::ShouldLookupHistory("reuse"),
                        "reuse 应查询历史") &&
                 Expect(!termtrans::history::ShouldLookupHistory("force"),
                        "force 不应查询历史") &&
                 Expect(!termtrans::history::ShouldLookupHistory("off"),
                        "off 不应查询历史") &&
                 Expect(termtrans::history::ShouldSaveHistory("ask"),
                        "ask 翻译成功后应保存历史") &&
                 Expect(termtrans::history::ShouldSaveHistory("reuse"),
                        "reuse 翻译成功后应保存历史") &&
                 Expect(termtrans::history::ShouldSaveHistory("force"),
                        "force 翻译成功后应保存历史") &&
                 Expect(!termtrans::history::ShouldSaveHistory("off"),
                        "off 不应保存历史")
             ? 0
             : 1;
}

/**
 * @brief 验证命中历史后的动作选择。
 */
int ShouldSelectHitAction() {
  return Expect(termtrans::history::DecideHistoryHitAction("reuse", true) ==
                    termtrans::history::HistoryHitAction::kReuse,
                "reuse 命中后应直接复用") &&
                 Expect(termtrans::history::DecideHistoryHitAction("force",
                                                                   true) ==
                            termtrans::history::HistoryHitAction::kTranslate,
                        "force 命中后应重新翻译") &&
                 Expect(termtrans::history::DecideHistoryHitAction("ask",
                                                                   true) ==
                            termtrans::history::HistoryHitAction::kAsk,
                        "ask 且可交互时应询问") &&
                 Expect(termtrans::history::DecideHistoryHitAction("ask",
                                                                   false) ==
                            termtrans::history::HistoryHitAction::kReuse,
                        "ask 但不可交互时应直接复用")
             ? 0
             : 1;
}

}  // 匿名命名空间

/**
 * @brief 运行历史策略测试。
 */
int main() {
  if (ShouldSelectLookupAndSaveBehavior() != 0) {
    return 1;
  }
  if (ShouldSelectHitAction() != 0) {
    return 1;
  }

  return 0;
}
