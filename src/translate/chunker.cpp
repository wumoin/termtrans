/**
 * @file chunker.cpp
 * @brief 实现翻译输入分块器。
 */

#include "translate/chunker.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace termtrans::translate {
namespace {

/**
 * @brief 判断文本是否只有空白字符。
 *
 * @param text 待检查文本。
 * @return 为空或全空白时返回 true。
 */
bool IsBlank(std::string_view text) {
  for (const char character : text) {
    if (std::isspace(static_cast<unsigned char>(character)) == 0) {
      return false;
    }
  }

  return true;
}

/**
 * @brief 将非空白片段加入结果集。
 *
 * @param text 原始文本。
 * @param begin 片段起始偏移。
 * @param end 片段结束偏移。
 * @param chunks 输出 chunk 列表，不能为空。
 */
void PushChunk(std::string_view text,
               std::size_t begin,
               std::size_t end,
               std::vector<std::string>* chunks) {
  if (end <= begin) {
    return;
  }

  std::string_view chunk = text.substr(begin, end - begin);
  if (!IsBlank(chunk)) {
    chunks->push_back(std::string(chunk));
  }
}

/**
 * @brief 判断字节是否为 UTF-8 continuation byte。
 *
 * @param byte 待检查字节。
 * @return 形如 10xxxxxx 时返回 true。
 */
bool IsUtf8ContinuationByte(unsigned char byte) {
  return (byte & 0xc0) == 0x80;
}

/**
 * @brief 从指定位置前进到下一个 UTF-8 字符边界。
 *
 * 当用户配置的 chunk 上限小于单个多字节字符时，必须允许当前 chunk
 * 略微超过上限，否则无法在合法 UTF-8 边界切分。
 *
 * @param text 完整输入文本。
 * @param begin 当前 chunk 起点，预期位于 UTF-8 字符边界。
 * @return 下一个字符边界。
 */
std::size_t NextUtf8Boundary(std::string_view text, std::size_t begin) {
  std::size_t end = std::min(text.size(), begin + 1);
  while (end < text.size() &&
         IsUtf8ContinuationByte(static_cast<unsigned char>(text[end]))) {
    ++end;
  }

  return end;
}

/**
 * @brief 将分割位置向前调整到 UTF-8 字符边界。
 *
 * 字节兜底只在找不到自然文本边界时触发。为了避免后续 JSON 序列化遇到
 * 非法 UTF-8，这里确保 chunk 不从多字节字符中间断开。
 *
 * @param text 完整输入文本。
 * @param begin 当前 chunk 起点。
 * @param end 原始分割位置。
 * @return 不小于 begin 的 UTF-8 安全分割位置。
 */
std::size_t MoveToUtf8Boundary(std::string_view text,
                               std::size_t begin,
                               std::size_t end) {
  while (end < text.size() && end > begin &&
         IsUtf8ContinuationByte(static_cast<unsigned char>(text[end]))) {
    --end;
  }

  if (end == begin) {
    return NextUtf8Boundary(text, begin);
  }

  return end;
}

/**
 * @brief 在目标上限前寻找优先分割点。
 *
 * 搜索优先级为：空行、普通换行、空格；都找不到时按字节上限兜底。
 * 字节兜底会向前调整到 UTF-8 字符边界，避免 provider JSON 请求体出现
 * 非法 UTF-8。
 *
 * @param text 完整输入文本。
 * @param begin 当前 chunk 起点。
 * @param hard_end 当前 chunk 最大结束位置。
 * @return 选定的结束位置。
 */
std::size_t FindSplitEnd(std::string_view text,
                         std::size_t begin,
                         std::size_t hard_end) {

  const std::size_t newline = text.rfind('\n', hard_end);
  if (newline != std::string_view::npos && newline >= begin) {
    return newline + 1;
  }

  const std::size_t space = text.rfind(' ', hard_end);
  if (space != std::string_view::npos && space > begin) {
    return space + 1;
  }

  return MoveToUtf8Boundary(text, begin, hard_end);
}

}  // namespace

/**
 * @brief 创建分块器。
 *
 * @param max_chunk_bytes 单个 chunk 的目标最大字节数，0 表示不限制。
 */
Chunker::Chunker(std::size_t max_chunk_bytes)
    : max_chunk_bytes_(max_chunk_bytes) {}

/**
 * @brief 按稳定边界切分输入文本。
 *
 * @param text 完整输入文本。
 * @return 非空 chunk 列表；全空白输入返回空列表。
 */
std::vector<std::string> Chunker::Split(std::string_view text) const {
  std::vector<std::string> chunks;
  if (IsBlank(text)) {
    return chunks;
  }
  if (max_chunk_bytes_ == 0) {
    chunks.push_back(std::string(text));
    return chunks;
  }

  std::size_t begin = 0;
  while (begin < text.size()) {
    const std::size_t hard_end =
        std::min(text.size(), begin + max_chunk_bytes_);
    if (hard_end == text.size()) {
      PushChunk(text, begin, text.size(), &chunks);
      break;
    }

    const std::size_t split_end = FindSplitEnd(text, begin, hard_end);
    PushChunk(text, begin, split_end, &chunks);
    begin = split_end;
  }

  return chunks;
}

}  // namespace termtrans::translate
