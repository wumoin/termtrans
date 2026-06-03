/**
 * @file platform_paths.h
 * @brief 声明配置与历史文件路径的跨平台解析入口。
 */

#pragma once

#include <filesystem>

namespace termtrans::config {

/**
 * @brief 返回默认配置文件路径。
 *
 * Linux 固定使用 ~/.config/termtrans/config.toml，不读取 XDG_CONFIG_HOME。
 * macOS 和 Windows 使用设计文档约定的平台默认目录。
 *
 * @return 当前平台默认配置文件路径。
 */
std::filesystem::path DefaultConfigPath();

/**
 * @brief 返回默认历史数据库路径。
 *
 * 历史数据库固定放在 config.toml 同目录，文件名为 history.sqlite，方便
 * 用户在同一个应用目录中查找配置和历史文件。
 *
 * @return 当前平台默认历史数据库路径。
 */
std::filesystem::path DefaultHistoryPath();

}  // 命名空间 termtrans::config
