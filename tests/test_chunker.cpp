/**
 * @file test_chunker.cpp
 * @brief 测试翻译输入分块器。
 */

#include "translate/chunker.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

bool Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    return false;
  }

  return true;
}

/**
 * @brief 验证短文本保持单 chunk。
 */
int ShouldKeepShortTextAsSingleChunk() {
  const termtrans::translate::Chunker chunker(100);
  const std::vector<std::string> chunks = chunker.Split("hello world");

  return Expect(chunks.size() == 1, "短文本应只有一个 chunk") &&
                 Expect(chunks.front() == "hello world",
                        "短文本 chunk 内容应不变")
             ? 0
             : 1;
}

/**
 * @brief 验证优先按空行边界切分。
 */
int ShouldSplitAtParagraphBoundary() {
  const termtrans::translate::Chunker chunker(14);
  const std::vector<std::string> chunks =
      chunker.Split("first line\n\nsecond line\n");

  return Expect(chunks.size() == 2, "应按空行切成两个 chunk") &&
                 Expect(chunks.front() == "first line\n\n",
                        "第一个 chunk 应包含段落边界")
             ? 0
             : 1;
}

/**
 * @brief 验证没有段落边界时按换行切分。
 */
int ShouldSplitAtLineBoundary() {
  const termtrans::translate::Chunker chunker(12);
  const std::vector<std::string> chunks =
      chunker.Split("line one\nline two\n");

  return Expect(chunks.size() == 2, "应按换行切分") &&
                 Expect(chunks.front() == "line one\n",
                        "第一个 chunk 应在换行后结束")
             ? 0
             : 1;
}

/**
 * @brief 验证长无边界文本使用字节兜底。
 */
int ShouldFallbackToByteBoundary() {
  const termtrans::translate::Chunker chunker(4);
  const std::vector<std::string> chunks = chunker.Split("abcdefghij");

  return Expect(chunks.size() == 3, "长无边界文本应按字节兜底") &&
                 Expect(chunks[0] == "abcd", "第一个兜底 chunk 应为 abcd") &&
                 Expect(chunks[2] == "ij", "最后一个 chunk 应保留剩余文本")
             ? 0
             : 1;
}

/**
 * @brief 验证 0 表示不按字节上限分块。
 */
int ShouldTreatZeroAsUnlimited() {
  const termtrans::translate::Chunker chunker(0);
  const std::vector<std::string> chunks =
      chunker.Split("first\nsecond\nthird\n");

  return Expect(chunks.size() == 1, "0 上限应保持单 chunk") &&
                 Expect(chunks.front() == "first\nsecond\nthird\n",
                        "0 上限不应切分非空输入")
             ? 0
             : 1;
}

/**
 * @brief 验证字节兜底不会切开 UTF-8 多字节字符。
 */
int ShouldPreserveUtf8CharacterBoundaries() {
  const std::string text =
      "\xE4\xBD\xA0\xE5\xA5\xBD\xE4\xB8\x96\xE7\x95\x8C";
  const termtrans::translate::Chunker chunker(1);
  const std::vector<std::string> chunks = chunker.Split(text);

  return Expect(chunks.size() == 4, "UTF-8 文本应按完整字符切分") &&
                 Expect(chunks[0] == "\xE4\xBD\xA0",
                        "第一个 chunk 应是完整的“你”") &&
                 Expect(chunks[1] == "\xE5\xA5\xBD",
                        "第二个 chunk 应是完整的“好”")
             ? 0
             : 1;
}

/**
 * @brief 验证全空白输入不产生 chunk。
 */
int ShouldDropBlankInput() {
  const termtrans::translate::Chunker chunker(10);
  const std::vector<std::string> chunks = chunker.Split(" \n\t ");

  return Expect(chunks.empty(), "全空白输入不应产生 chunk") ? 0 : 1;
}

/**
 * @brief 验证代码块缩进不会被主动修剪。
 */
int ShouldPreserveIndentedText() {
  const termtrans::translate::Chunker chunker(100);
  const std::vector<std::string> chunks =
      chunker.Split("code:\n    int value = 1;\n");

  return Expect(chunks.size() == 1, "缩进文本应保持单 chunk") &&
                 Expect(chunks.front().find("    int") != std::string::npos,
                        "chunk 应保留代码缩进")
             ? 0
             : 1;
}

}  // namespace

int main() {
  if (ShouldKeepShortTextAsSingleChunk() != 0) {
    return 1;
  }
  if (ShouldSplitAtParagraphBoundary() != 0) {
    return 1;
  }
  if (ShouldSplitAtLineBoundary() != 0) {
    return 1;
  }
  if (ShouldFallbackToByteBoundary() != 0) {
    return 1;
  }
  if (ShouldTreatZeroAsUnlimited() != 0) {
    return 1;
  }
  if (ShouldPreserveUtf8CharacterBoundaries() != 0) {
    return 1;
  }
  if (ShouldDropBlankInput() != 0) {
    return 1;
  }
  if (ShouldPreserveIndentedText() != 0) {
    return 1;
  }

  return 0;
}
