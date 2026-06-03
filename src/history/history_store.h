/**
 * @file history_store.h
 * @brief 声明翻译历史存储抽象接口。
 */

#pragma once

#include "history/history_key.h"
#include "history/history_record.h"

#include <string>
#include <vector>

namespace termtrans::history {

/**
 * @brief 历史查询结果。
 */
struct HistoryLookupResult {
  bool is_ok = false;  // 存储查询是否成功。
  bool has_record = false;  // 是否命中历史记录。
  HistoryRecord record;  // 命中时的完整历史记录。
  std::string error_message;  // 查询失败时的内部错误文本。
};

/**
 * @brief 历史列表查询结果。
 */
struct HistoryListResult {
  bool is_ok = false;  // 列表查询是否成功。
  std::vector<HistoryRecord> records;  // 按最近更新时间倒序排列的记录。
  std::string error_message;  // 查询失败时的内部错误文本。
};

/**
 * @brief 历史写入或清理结果。
 */
struct HistoryMutationResult {
  bool is_ok = false;  // 写入或清理是否成功。
  std::string error_message;  // 失败时的内部错误文本。
};

/**
 * @brief 翻译历史存储接口。
 *
 * HistoryStore 只负责本地历史记录读写，不决定历史命中策略，也不参与
 * provider 调用或 stdout 输出。
 *
 * @note 实现类自行声明线程安全性。
 */
class HistoryStore {
 public:
  virtual ~HistoryStore() = default;

  /**
   * @brief 按历史 key 查询一条记录。
   *
   * @param key 只包含 input_sha256 和 target_language 的历史 key。
   * @return 查询结果；未命中不是错误。
   */
  virtual HistoryLookupResult Lookup(const HistoryKey& key) = 0;

  /**
   * @brief 保存或替换一条历史记录。
   *
   * @param record 待保存记录；created_at 和 updated_at 由实现维护。
   * @return 写入结果。
   */
  virtual HistoryMutationResult Save(const HistoryRecord& record) = 0;

  /**
   * @brief 列出当前全部历史记录。
   *
   * @return 按最近更新时间倒序排列的记录列表。
   */
  virtual HistoryListResult List() = 0;

  /**
   * @brief 清空全部历史记录。
   *
   * @return 清理结果。
   */
  virtual HistoryMutationResult Clear() = 0;
};

}  // namespace termtrans::history
