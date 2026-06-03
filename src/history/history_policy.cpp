/**
 * @file history_policy.cpp
 * @brief 实现历史命中策略判断。
 */

#include "history/history_policy.h"

#include <string_view>

namespace termtrans::history {

/**
 * @brief 判断当前历史模式是否应查询存储。
 */
bool ShouldLookupHistory(std::string_view history_mode) {
  return history_mode != "off" && history_mode != "force";
}

/**
 * @brief 判断当前历史模式下翻译成功后是否应保存历史。
 */
bool ShouldSaveHistory(std::string_view history_mode) {
  return history_mode != "off";
}

/**
 * @brief 根据历史模式和交互能力决定命中后的行为。
 */
HistoryHitAction DecideHistoryHitAction(std::string_view history_mode,
                                        bool can_ask) {
  if (history_mode == "force") {
    return HistoryHitAction::kTranslate;
  }

  if (history_mode == "ask") {
    return can_ask ? HistoryHitAction::kAsk : HistoryHitAction::kReuse;
  }

  return HistoryHitAction::kReuse;
}

}  // namespace termtrans::history
