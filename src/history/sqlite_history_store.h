/**
 * @file sqlite_history_store.h
 * @brief 声明基于 SQLite 的翻译历史存储。
 */

#pragma once

#include "history/history_store.h"

#include <filesystem>

struct sqlite3;

namespace termtrans::history {

/**
 * @brief 使用本地 SQLite 文件保存翻译历史。
 *
 * SqliteHistoryStore 负责创建历史数据库、维护 records 表和执行 upsert。
 * 它不保存 API key，不执行历史命中策略，也不负责向用户输出历史记录。
 *
 * @note 该类不是线程安全的。
 */
class SqliteHistoryStore : public HistoryStore {
 public:
  /**
   * @brief 创建 SQLite 历史存储。
   *
   * @param path 历史数据库文件路径；父目录会在首次访问时创建。
   */
  explicit SqliteHistoryStore(std::filesystem::path path);

  /**
   * @brief 关闭 SQLite 连接。
   */
  ~SqliteHistoryStore() override;

  SqliteHistoryStore(const SqliteHistoryStore&) = delete;
  SqliteHistoryStore& operator=(const SqliteHistoryStore&) = delete;

  /**
   * @brief 按历史 key 查询一条记录。
   */
  HistoryLookupResult Lookup(const HistoryKey& key) override;

  /**
   * @brief 保存或替换一条历史记录。
   */
  HistoryMutationResult Save(const HistoryRecord& record) override;

  /**
   * @brief 列出全部历史记录。
   */
  HistoryListResult List() override;

  /**
   * @brief 清空全部历史记录。
   */
  HistoryMutationResult Clear() override;

 private:
  std::filesystem::path path_;  // SQLite 数据库文件路径。
  sqlite3* database_ = nullptr;  // SQLite 连接；由本对象拥有。
};

}  // namespace termtrans::history
