/**
 * @file test_history_store.cpp
 * @brief 测试 SQLite 翻译历史存储。
 */

#include "history/history_key.h"
#include "history/history_record.h"
#include "history/sqlite_history_store.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

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
 * @brief 判断文本是否包含期望片段。
 */
bool ExpectContains(const std::string& text,
                    const std::string& expected,
                    const char* message) {
  if (text.find(expected) == std::string::npos) {
    std::cerr << message << '\n';
    return false;
  }

  return true;
}

/**
 * @brief 创建历史存储测试使用的临时目录。
 */
std::filesystem::path MakeTempDirectory() {
  const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() /
      ("termtrans_history_store_test_" + std::to_string(tick));
  std::filesystem::create_directories(path);
  return path;
}

/**
 * @brief 读取二进制文件内容。
 */
std::string ReadBinary(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(input),
                     std::istreambuf_iterator<char>());
}

/**
 * @brief 构造测试历史记录。
 */
termtrans::history::HistoryRecord MakeRecord(std::string input_text,
                                             std::string translated_text) {
  return termtrans::history::HistoryRecord{
      .key = termtrans::history::ComputeHistoryKey(input_text, "zh-CN"),
      .input_text = input_text,
      .translated_text = translated_text,
      .model_name = "deepseek",
      .provider_type = "openai-compatible",
      .model_id = "deepseek-chat",
      .base_url = "https://api.deepseek.com",
      .input_bytes = input_text.size(),
      .output_bytes = translated_text.size(),
  };
}

/**
 * @brief 验证保存和查询历史记录。
 */
int ShouldSaveAndLookupRecord() {
  const std::filesystem::path database_path =
      MakeTempDirectory() / "history.sqlite";
  termtrans::history::SqliteHistoryStore store(database_path);

  const termtrans::history::HistoryRecord record =
      MakeRecord("hello", "你好");
  const termtrans::history::HistoryMutationResult save_result =
      store.Save(record);
  const termtrans::history::HistoryLookupResult lookup_result =
      store.Lookup(record.key);

  return Expect(save_result.is_ok, "保存历史记录应成功") &&
                 Expect(lookup_result.is_ok, "查询历史记录应成功") &&
                 Expect(lookup_result.has_record, "应命中已保存记录") &&
                 Expect(lookup_result.record.translated_text == "你好",
                        "应读取完整译文") &&
                 Expect(lookup_result.record.model_name == "deepseek",
                        "应读取模型名称") &&
                 Expect(lookup_result.record.input_bytes == 5,
                        "应保存输入字节数") &&
                 Expect(lookup_result.record.output_bytes == 6,
                        "UTF-8 译文字节数应保存为原始字节长度")
             ? 0
             : 1;
}

/**
 * @brief 验证相同 key 保存会更新记录而不是插入重复项。
 */
int ShouldUpsertByHistoryKey() {
  const std::filesystem::path database_path =
      MakeTempDirectory() / "history.sqlite";
  termtrans::history::SqliteHistoryStore store(database_path);

  termtrans::history::HistoryRecord record = MakeRecord("hello", "你好");
  termtrans::history::HistoryRecord updated = record;
  updated.translated_text = "您好";
  updated.output_bytes = updated.translated_text.size();
  updated.model_name = "local";

  const termtrans::history::HistoryMutationResult first_save =
      store.Save(record);
  const termtrans::history::HistoryMutationResult second_save =
      store.Save(updated);
  const termtrans::history::HistoryLookupResult lookup_result =
      store.Lookup(record.key);
  const termtrans::history::HistoryListResult list_result = store.List();

  return Expect(first_save.is_ok, "首次保存应成功") &&
                 Expect(second_save.is_ok, "重复 key 保存应成功") &&
                 Expect(lookup_result.is_ok && lookup_result.has_record,
                        "更新后应仍可命中记录") &&
                 Expect(lookup_result.record.translated_text == "您好",
                        "upsert 应更新译文") &&
                 Expect(lookup_result.record.model_name == "local",
                        "upsert 应更新模型元数据") &&
                 Expect(list_result.is_ok, "列表查询应成功") &&
                 Expect(list_result.records.size() == 1,
                        "相同 key 不应产生重复记录")
             ? 0
             : 1;
}

/**
 * @brief 验证列表和清空历史记录。
 */
int ShouldListAndClearRecords() {
  const std::filesystem::path database_path =
      MakeTempDirectory() / "history.sqlite";
  termtrans::history::SqliteHistoryStore store(database_path);

  const termtrans::history::HistoryMutationResult first_save =
      store.Save(MakeRecord("hello", "你好"));
  const termtrans::history::HistoryMutationResult second_save =
      store.Save(MakeRecord("world", "世界"));
  const termtrans::history::HistoryListResult list_result = store.List();
  const termtrans::history::HistoryMutationResult clear_result = store.Clear();
  const termtrans::history::HistoryListResult empty_result = store.List();

  return Expect(first_save.is_ok && second_save.is_ok,
                "保存两条记录应成功") &&
                 Expect(list_result.is_ok, "列表查询应成功") &&
                 Expect(list_result.records.size() == 2,
                        "列表应返回全部记录") &&
                 Expect(clear_result.is_ok, "清空历史应成功") &&
                 Expect(empty_result.is_ok, "清空后列表查询应成功") &&
                 Expect(empty_result.records.empty(),
                        "清空后列表应为空")
             ? 0
             : 1;
}

/**
 * @brief 验证历史数据库不保存 API key 明文。
 */
int ShouldNotStoreApiKey() {
  const std::filesystem::path database_path =
      MakeTempDirectory() / "history.sqlite";
  termtrans::history::SqliteHistoryStore store(database_path);

  const termtrans::history::HistoryRecord record =
      MakeRecord("hello", "你好");
  const termtrans::history::HistoryMutationResult save_result =
      store.Save(record);
  const std::string database_bytes = ReadBinary(database_path);

  return Expect(save_result.is_ok, "保存历史记录应成功") &&
                 Expect(database_bytes.find("test-token-secret") ==
                            std::string::npos,
                        "历史数据库不应包含测试 API key") &&
                 ExpectContains(database_bytes, "deepseek-chat",
                                "历史数据库应包含非敏感模型元数据")
             ? 0
             : 1;
}

}  // 匿名命名空间

/**
 * @brief 运行 SQLite 历史存储测试。
 */
int main() {
  if (ShouldSaveAndLookupRecord() != 0) {
    return 1;
  }
  if (ShouldUpsertByHistoryKey() != 0) {
    return 1;
  }
  if (ShouldListAndClearRecords() != 0) {
    return 1;
  }
  if (ShouldNotStoreApiKey() != 0) {
    return 1;
  }

  return 0;
}
