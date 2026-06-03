/**
 * @file sqlite_history_store.cpp
 * @brief 实现基于 SQLite 的翻译历史存储。
 */

#include "history/sqlite_history_store.h"

#include <cstdint>
#include <ctime>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <sqlite3.h>

namespace termtrans::history {
namespace {

/**
 * @brief SQLite statement RAII 封装。
 *
 * @note 该类不是线程安全的。
 */
class Statement {
 public:
  /**
   * @brief 编译 SQL statement。
   *
   * @param database 已打开 SQLite 连接，不能为空。
   * @param sql 待编译 SQL。
   */
  Statement(sqlite3* database, std::string_view sql) : database_(database) {
    if (database_ == nullptr) {
      return;
    }

    sqlite3_prepare_v2(database_, sql.data(), static_cast<int>(sql.size()),
                       &statement_, nullptr);
  }

  /**
   * @brief 释放 statement。
   */
  ~Statement() {
    if (statement_ != nullptr) {
      sqlite3_finalize(statement_);
    }
  }

  Statement(const Statement&) = delete;
  Statement& operator=(const Statement&) = delete;

  sqlite3_stmt* get() const { return statement_; }

 private:
  sqlite3* database_ = nullptr;  // 不拥有；statement 所属 SQLite 连接。
  sqlite3_stmt* statement_ = nullptr;  // SQLite statement；由本对象拥有。
};

/**
 * @brief 生成 SQLite 失败结果文本。
 *
 * @param database SQLite 连接。
 * @return 当前连接最后一次错误文本。
 */
std::string SqliteError(sqlite3* database) {
  if (database == nullptr) {
    return "sqlite database is not open";
  }

  const char* message = sqlite3_errmsg(database);
  return message == nullptr ? "sqlite error" : std::string(message);
}

/**
 * @brief 执行不带返回行的 SQL。
 *
 * @param database SQLite 连接。
 * @param sql 待执行 SQL。
 * @return 成功时返回空，失败时返回 SQLite 错误文本。
 */
std::optional<std::string> Execute(sqlite3* database, const char* sql) {
  char* error_message = nullptr;
  const int result = sqlite3_exec(database, sql, nullptr, nullptr,
                                  &error_message);
  if (result == SQLITE_OK) {
    return std::nullopt;
  }

  std::string error =
      error_message == nullptr ? SqliteError(database) : error_message;
  sqlite3_free(error_message);
  return error;
}

/**
 * @brief 读取 SQLite 文本列。
 *
 * @param statement 当前行 statement。
 * @param column 列序号。
 * @return UTF-8 文本；NULL 视为空字符串。
 */
std::string ColumnText(sqlite3_stmt* statement, int column) {
  const unsigned char* text = sqlite3_column_text(statement, column);
  if (text == nullptr) {
    return "";
  }

  return reinterpret_cast<const char*>(text);
}

/**
 * @brief 将无符号字节数绑定到 SQLite 整数字段。
 *
 * SQLite 使用有符号 64 位整数；历史记录中的字节数字段来自 string::size，
 * 现实输入不应接近 int64 上限。这里做显式转换，保持跨平台类型清晰。
 *
 * @param value 待绑定字节数。
 * @return SQLite int64 值。
 */
sqlite3_int64 ToSqliteInt64(std::uint64_t value) {
  return static_cast<sqlite3_int64>(value);
}

/**
 * @brief 从当前 SQLite 行构造历史记录。
 *
 * @param statement 当前已定位到有效行的 statement。
 * @return 历史记录。
 */
HistoryRecord ReadRecord(sqlite3_stmt* statement) {
  return HistoryRecord{
      .key =
          HistoryKey{
              .input_sha256 = ColumnText(statement, 0),
              .target_language = ColumnText(statement, 1),
          },
      .input_text = ColumnText(statement, 2),
      .translated_text = ColumnText(statement, 3),
      .model_name = ColumnText(statement, 4),
      .provider_type = ColumnText(statement, 5),
      .model_id = ColumnText(statement, 6),
      .base_url = ColumnText(statement, 7),
      .created_at = ColumnText(statement, 8),
      .updated_at = ColumnText(statement, 9),
      .input_bytes = static_cast<std::uint64_t>(
          sqlite3_column_int64(statement, 10)),
      .output_bytes = static_cast<std::uint64_t>(
          sqlite3_column_int64(statement, 11)),
  };
}

/**
 * @brief 返回当前 UTC 时间的 ISO-8601 文本。
 *
 * SQLite 存储 ISO 文本便于 history list 直接展示，同时不依赖平台本地
 * 时区格式。
 *
 * @return 形如 YYYY-MM-DDTHH:MM:SSZ 的 UTC 时间。
 */
std::string CurrentUtcTimestamp() {
  std::time_t now = std::time(nullptr);
  std::tm utc_time{};
#ifdef _WIN32
  gmtime_s(&utc_time, &now);
#else
  gmtime_r(&now, &utc_time);
#endif

  char buffer[32] = {};
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc_time);
  return buffer;
}

/**
 * @brief 绑定 SQLite 文本字段。
 *
 * @param statement 已编译 statement。
 * @param index 1-based 参数序号。
 * @param value 待绑定文本。
 * @return SQLite 返回码。
 */
int BindText(sqlite3_stmt* statement, int index, const std::string& value) {
  return sqlite3_bind_text(statement, index, value.c_str(),
                           static_cast<int>(value.size()), SQLITE_TRANSIENT);
}

/**
 * @brief 创建历史表和索引。
 *
 * @param database SQLite 连接。
 * @return 初始化结果。
 */
HistoryMutationResult InitializeSchema(sqlite3* database) {
  const char* sql = R"sql(
CREATE TABLE IF NOT EXISTS records (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  input_sha256 TEXT NOT NULL,
  target_language TEXT NOT NULL,
  input_text TEXT NOT NULL,
  translated_text TEXT NOT NULL,
  model_name TEXT NOT NULL,
  provider_type TEXT NOT NULL,
  model_id TEXT NOT NULL,
  base_url TEXT NOT NULL,
  created_at TEXT NOT NULL,
  updated_at TEXT NOT NULL,
  input_bytes INTEGER NOT NULL,
  output_bytes INTEGER NOT NULL,
  UNIQUE(input_sha256, target_language)
);
CREATE INDEX IF NOT EXISTS idx_records_updated_at ON records(updated_at);
)sql";

  const std::optional<std::string> error = Execute(database, sql);
  if (error.has_value()) {
    return HistoryMutationResult{.is_ok = false, .error_message = *error};
  }

  return HistoryMutationResult{.is_ok = true};
}

}  // namespace

/**
 * @brief 创建 SQLite 历史存储。
 */
SqliteHistoryStore::SqliteHistoryStore(std::filesystem::path path)
    : path_(std::move(path)) {}

/**
 * @brief 关闭 SQLite 连接。
 */
SqliteHistoryStore::~SqliteHistoryStore() {
  if (database_ != nullptr) {
    sqlite3_close(database_);
    database_ = nullptr;
  }
}

/**
 * @brief 确保 SQLite 连接已打开并完成 schema 初始化。
 *
 * 该函数放在类外只为了避免在头文件暴露 sqlite3 细节；调用方仍通过
 * SqliteHistoryStore 的 public 方法触发。
 *
 * @param database_path 数据库路径。
 * @param database SQLite 连接指针地址。
 * @return 初始化结果。
 */
HistoryMutationResult EnsureOpen(const std::filesystem::path& database_path,
                                 sqlite3** database) {
  if (*database != nullptr) {
    return HistoryMutationResult{.is_ok = true};
  }

  std::error_code directory_error;
  std::filesystem::create_directories(database_path.parent_path(),
                                      directory_error);
  if (directory_error) {
    return HistoryMutationResult{
        .is_ok = false,
        .error_message = directory_error.message(),
    };
  }

  sqlite3* opened_database = nullptr;
  const int open_result =
      sqlite3_open(database_path.string().c_str(), &opened_database);
  if (open_result != SQLITE_OK) {
    std::string error = SqliteError(opened_database);
    if (opened_database != nullptr) {
      sqlite3_close(opened_database);
    }
    return HistoryMutationResult{.is_ok = false, .error_message = error};
  }

  *database = opened_database;
  return InitializeSchema(*database);
}

/**
 * @brief 按历史 key 查询一条记录。
 */
HistoryLookupResult SqliteHistoryStore::Lookup(const HistoryKey& key) {
  const HistoryMutationResult open_result = EnsureOpen(path_, &database_);
  if (!open_result.is_ok) {
    return HistoryLookupResult{
        .is_ok = false,
        .error_message = open_result.error_message,
    };
  }

  const char* sql = R"sql(
SELECT input_sha256, target_language, input_text, translated_text,
       model_name, provider_type, model_id, base_url, created_at, updated_at,
       input_bytes, output_bytes
FROM records
WHERE input_sha256 = ? AND target_language = ?
LIMIT 1
)sql";
  Statement statement(database_, sql);
  if (statement.get() == nullptr) {
    return HistoryLookupResult{.is_ok = false,
                               .error_message = SqliteError(database_)};
  }

  BindText(statement.get(), 1, key.input_sha256);
  BindText(statement.get(), 2, key.target_language);

  const int step_result = sqlite3_step(statement.get());
  if (step_result == SQLITE_ROW) {
    return HistoryLookupResult{
        .is_ok = true,
        .has_record = true,
        .record = ReadRecord(statement.get()),
    };
  }

  if (step_result == SQLITE_DONE) {
    return HistoryLookupResult{.is_ok = true};
  }

  return HistoryLookupResult{.is_ok = false,
                             .error_message = SqliteError(database_)};
}

/**
 * @brief 保存或替换一条历史记录。
 */
HistoryMutationResult SqliteHistoryStore::Save(const HistoryRecord& record) {
  const HistoryMutationResult open_result = EnsureOpen(path_, &database_);
  if (!open_result.is_ok) {
    return open_result;
  }

  const char* sql = R"sql(
INSERT INTO records (
  input_sha256, target_language, input_text, translated_text,
  model_name, provider_type, model_id, base_url, created_at, updated_at,
  input_bytes, output_bytes
) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
ON CONFLICT(input_sha256, target_language) DO UPDATE SET
  input_text = excluded.input_text,
  translated_text = excluded.translated_text,
  model_name = excluded.model_name,
  provider_type = excluded.provider_type,
  model_id = excluded.model_id,
  base_url = excluded.base_url,
  updated_at = excluded.updated_at,
  input_bytes = excluded.input_bytes,
  output_bytes = excluded.output_bytes
)sql";

  Statement statement(database_, sql);
  if (statement.get() == nullptr) {
    return HistoryMutationResult{.is_ok = false,
                                 .error_message = SqliteError(database_)};
  }

  const std::string now = CurrentUtcTimestamp();
  const std::string created_at = record.created_at.empty() ? now
                                                           : record.created_at;
  const std::string updated_at = record.updated_at.empty() ? now
                                                           : record.updated_at;

  BindText(statement.get(), 1, record.key.input_sha256);
  BindText(statement.get(), 2, record.key.target_language);
  BindText(statement.get(), 3, record.input_text);
  BindText(statement.get(), 4, record.translated_text);
  BindText(statement.get(), 5, record.model_name);
  BindText(statement.get(), 6, record.provider_type);
  BindText(statement.get(), 7, record.model_id);
  BindText(statement.get(), 8, record.base_url);
  BindText(statement.get(), 9, created_at);
  BindText(statement.get(), 10, updated_at);
  sqlite3_bind_int64(statement.get(), 11, ToSqliteInt64(record.input_bytes));
  sqlite3_bind_int64(statement.get(), 12, ToSqliteInt64(record.output_bytes));

  const int step_result = sqlite3_step(statement.get());
  if (step_result != SQLITE_DONE) {
    return HistoryMutationResult{.is_ok = false,
                                 .error_message = SqliteError(database_)};
  }

  return HistoryMutationResult{.is_ok = true};
}

/**
 * @brief 列出全部历史记录。
 */
HistoryListResult SqliteHistoryStore::List() {
  const HistoryMutationResult open_result = EnsureOpen(path_, &database_);
  if (!open_result.is_ok) {
    return HistoryListResult{
        .is_ok = false,
        .error_message = open_result.error_message,
    };
  }

  const char* sql = R"sql(
SELECT input_sha256, target_language, input_text, translated_text,
       model_name, provider_type, model_id, base_url, created_at, updated_at,
       input_bytes, output_bytes
FROM records
ORDER BY updated_at DESC, id DESC
)sql";
  Statement statement(database_, sql);
  if (statement.get() == nullptr) {
    return HistoryListResult{.is_ok = false,
                             .error_message = SqliteError(database_)};
  }

  HistoryListResult result{.is_ok = true};
  for (;;) {
    const int step_result = sqlite3_step(statement.get());
    if (step_result == SQLITE_DONE) {
      return result;
    }
    if (step_result != SQLITE_ROW) {
      return HistoryListResult{.is_ok = false,
                               .error_message = SqliteError(database_)};
    }

    result.records.push_back(ReadRecord(statement.get()));
  }
}

/**
 * @brief 清空全部历史记录。
 */
HistoryMutationResult SqliteHistoryStore::Clear() {
  const HistoryMutationResult open_result = EnsureOpen(path_, &database_);
  if (!open_result.is_ok) {
    return open_result;
  }

  const std::optional<std::string> error = Execute(database_, "DELETE FROM records");
  if (error.has_value()) {
    return HistoryMutationResult{.is_ok = false, .error_message = *error};
  }

  return HistoryMutationResult{.is_ok = true};
}

}  // namespace termtrans::history
