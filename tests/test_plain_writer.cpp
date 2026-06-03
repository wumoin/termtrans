/**
 * @file test_plain_writer.cpp
 * @brief 测试 stdout 纯译文输出器。
 */

#include "output/plain_writer.h"

#include <iostream>
#include <sstream>
#include <streambuf>
#include <string>

namespace {

bool Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    return false;
  }

  return true;
}

/**
 * @brief 总是失败的输出缓冲区。
 */
class FailingStreambuf : public std::streambuf {
 protected:
  int overflow(int character) override { return traits_type::eof(); }
};

/**
 * @brief 验证写入成功时 stdout 只包含译文。
 */
int ShouldWritePlainText() {
  std::ostringstream output;
  termtrans::output::PlainWriter writer(output);

  const bool first = writer.Write("hello");
  const bool second = writer.Write(" world");

  return Expect(first, "第一次写入应成功") &&
                 Expect(second, "第二次写入应成功") &&
                 Expect(output.str() == "hello world",
                        "PlainWriter 应只写入译文文本") &&
                 Expect(!writer.IsCancelled(), "成功写入不应标记取消")
             ? 0
             : 1;
}

/**
 * @brief 验证输出流失败会标记取消。
 */
int ShouldCancelOnWriteFailure() {
  FailingStreambuf buffer;
  std::ostream output(&buffer);
  termtrans::output::PlainWriter writer(output);

  const bool result = writer.Write("hello");

  return Expect(!result, "写入失败应返回 false") &&
                 Expect(writer.IsCancelled(), "写入失败应标记取消")
             ? 0
             : 1;
}

}  // namespace

int main() {
  if (ShouldWritePlainText() != 0) {
    return 1;
  }
  if (ShouldCancelOnWriteFailure() != 0) {
    return 1;
  }

  return 0;
}
