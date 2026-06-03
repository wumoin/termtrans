/**
 * @file history_policy.h
 * @brief 声明历史命中策略判断。
 */

#pragma once

#include <string_view>

namespace termtrans::history {

/**
 * @brief 命中历史后的下一步动作。
 */
enum class HistoryHitAction {
  kReuse,  // 直接复用历史译文。
  kAsk,  // 需要询问用户是否重新翻译。
  kTranslate,  // 跳过历史译文并重新翻译。
};

/**
 * @brief 判断当前历史模式是否应查询存储。
 *
 * @param history_mode 已解析历史模式：ask、reuse、force 或 off。
 * @return 需要查询历史时返回 true。
 */
bool ShouldLookupHistory(std::string_view history_mode);

/**
 * @brief 判断当前历史模式下翻译成功后是否应保存历史。
 *
 * @param history_mode 已解析历史模式：ask、reuse、force 或 off。
 * @return 应保存历史时返回 true。
 */
bool ShouldSaveHistory(std::string_view history_mode);

/**
 * @brief 根据历史模式和交互能力决定命中后的行为。
 *
 * @param history_mode 已解析历史模式：ask、reuse、force 或 off。
 * @param can_ask 当前进程是否能够向用户询问。
 * @return 命中历史后的下一步动作。
 */
HistoryHitAction DecideHistoryHitAction(std::string_view history_mode,
                                        bool can_ask);

}  // namespace termtrans::history
