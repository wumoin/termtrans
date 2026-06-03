/**
 * @file platform_paths.cpp
 * @brief 实现配置与历史文件路径的跨平台解析。
 */

#include "config/platform_paths.h"

#include <cstdlib>
#include <filesystem>
#include <string>

namespace termtrans::config {
namespace {

/**
 * @brief 将环境变量转换为文件系统路径。
 *
 * 空字符串和未设置变量都视为不可用路径，避免后续拼接出相对配置
 * 目录。
 *
 * @param name 环境变量名，不能为空。
 * @return 环境变量对应路径；变量不可用时返回空路径。
 */
std::filesystem::path EnvironmentPath(const char* name) {
  const char* value = std::getenv(name);
  if (value == nullptr || std::string(value).empty()) {
    return {};
  }

  return std::filesystem::path(value);
}

/**
 * @brief 获取当前用户主目录。
 *
 * Windows 优先读取 USERPROFILE，类 Unix 平台读取 HOME。该函数只服务配置
 * 路径拼接，不负责创建目录或验证路径是否存在。
 *
 * @return 当前平台可推断的用户主目录；缺失时返回空路径。
 */
std::filesystem::path HomeDirectory() {
#ifdef _WIN32
  std::filesystem::path user_profile = EnvironmentPath("USERPROFILE");
  if (!user_profile.empty()) {
    return user_profile;
  }
#endif

  return EnvironmentPath("HOME");
}

}  // 匿名命名空间

/**
 * @brief 返回当前平台默认配置文件路径。
 *
 * Linux 路径按阶段 1 决策固定为 HOME 下的 `.config`，不读取 XDG_CONFIG_HOME；
 * Windows 在 APPDATA 缺失时回退 USERPROFILE 下的 Roaming 目录。
 *
 * @return 默认配置文件路径；环境变量缺失时可能包含空路径前缀。
 */
std::filesystem::path DefaultConfigPath() {
#ifdef _WIN32
  std::filesystem::path app_data = EnvironmentPath("APPDATA");
  if (app_data.empty()) {
    app_data = HomeDirectory() / "AppData" / "Roaming";
  }
  return app_data / "termtrans" / "config.toml";
#elif defined(__APPLE__)
  return HomeDirectory() / "Library" / "Application Support" / "termtrans" /
         "config.toml";
#else
  return HomeDirectory() / ".config" / "termtrans" / "config.toml";
#endif
}

/**
 * @brief 返回当前平台默认历史数据库路径。
 *
 * 历史数据库与 config.toml 共用目录，但使用独立文件名，避免额外数据
 * 目录规则增加用户查找成本。
 *
 * @return 默认历史数据库路径。
 */
std::filesystem::path DefaultHistoryPath() {
  return DefaultConfigPath().parent_path() / "history.sqlite";
}

}  // 命名空间 termtrans::config
