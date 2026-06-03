/**
 * @file app_constants.h
 * @brief 定义应用外壳和配置合并使用的手写编译期默认值。
 */

#pragma once

#include <cstdint>

namespace termtrans::config::constants::app {

inline constexpr char kApplicationName[] = "termtrans";  // CLI 二进制名称。
inline constexpr char kDefaultTargetLanguage[] = "en";  // 兜底目标翻译语言。
// 系统语言未命中时的兜底界面语言。
inline constexpr char kDefaultUiLanguage[] = "en";
inline constexpr bool kDefaultStream = true;  // 默认将译文增量写入 stdout。
inline constexpr char kDefaultHistoryMode[] = "ask";  // 默认命中历史时询问用户。
// 0 表示不按用户阈值限制输入。
inline constexpr std::uint64_t kDefaultMaxInputBytes = 0;

}  // 命名空间 termtrans::config::constants::app
