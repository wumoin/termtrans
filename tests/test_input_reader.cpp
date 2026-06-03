/**
 * @file test_input_reader.cpp
 * @brief 测试翻译输入读取规则。
 */

#include "input/reader.h"

#include <iostream>
#include <sstream>
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
 * @brief 验证 stdin 非交互时优先读取 stdin。
 */
int ShouldReadFromNonInteractiveStdin() {
  std::istringstream stdin_stream("stdin text");

  const termtrans::input::ReadInputResult result = termtrans::input::ReadInput(
      {"direct text"}, stdin_stream,
      termtrans::input::ReadInputOptions{
          .is_stdin_interactive = false,
      });

  return Expect(result.is_ok, "stdin 非交互时应读取成功") &&
                 Expect(result.text == "stdin text",
                        "stdin 应优先于直接文本参数")
             ? 0
             : 1;
}

/**
 * @brief 验证 stdin 交互时接受唯一直接文本。
 */
int ShouldReadSingleDirectText() {
  std::istringstream stdin_stream;

  const termtrans::input::ReadInputResult result = termtrans::input::ReadInput(
      {"hello world"}, stdin_stream,
      termtrans::input::ReadInputOptions{
          .is_stdin_interactive = true,
      });

  return Expect(result.is_ok, "唯一直接文本应读取成功") &&
                 Expect(result.text == "hello world",
                        "应返回直接文本本身")
             ? 0
             : 1;
}

/**
 * @brief 验证空输入和多个直接文本会失败。
 */
int ShouldRejectEmptyAndMultipleDirectText() {
  std::istringstream stdin_stream;

  const termtrans::input::ReadInputResult empty_result =
      termtrans::input::ReadInput({}, stdin_stream,
                                  termtrans::input::ReadInputOptions{
                                      .is_stdin_interactive = true,
                                  });
  const termtrans::input::ReadInputResult multiple_result =
      termtrans::input::ReadInput({"hello", "world"}, stdin_stream,
                                  termtrans::input::ReadInputOptions{
                                      .is_stdin_interactive = true,
                                  });

  return Expect(!empty_result.is_ok, "空输入不应成功") &&
                 Expect(empty_result.error_kind ==
                            termtrans::input::ReadInputErrorKind::kEmptyInput,
                        "空输入应返回 EmptyInput") &&
                 Expect(!multiple_result.is_ok,
                        "多个直接文本参数不应成功") &&
                 Expect(multiple_result.error_kind ==
                            termtrans::input::ReadInputErrorKind::kInvalidUsage,
                        "多个直接文本应返回 InvalidUsage")
             ? 0
             : 1;
}

/**
 * @brief 验证输入大小不阻止非交互 stdin 进入分块翻译流程。
 */
int ShouldAllowLargeNonInteractiveInput() {
  std::istringstream stdin_stream("too long");

  const termtrans::input::ReadInputResult result = termtrans::input::ReadInput(
      {}, stdin_stream,
      termtrans::input::ReadInputOptions{
          .is_stdin_interactive = false,
      });

  return Expect(result.is_ok, "大输入应读取成功并交给后续分块流程") &&
                 Expect(result.text == "too long", "应返回完整输入文本")
             ? 0
             : 1;
}

/**
 * @brief 验证直接文本大小不触发交互确认。
 */
int ShouldAllowLargeDirectTextWithoutConfirmation() {
  std::istringstream stdin_stream;

  const termtrans::input::ReadInputResult result = termtrans::input::ReadInput(
      {"too long"}, stdin_stream,
      termtrans::input::ReadInputOptions{
          .is_stdin_interactive = true,
      });

  return Expect(result.is_ok, "大直接文本应读取成功") &&
                 Expect(result.text == "too long", "应返回完整直接文本")
             ? 0
             : 1;
}

}  // namespace

int main() {
  if (ShouldReadFromNonInteractiveStdin() != 0) {
    return 1;
  }
  if (ShouldReadSingleDirectText() != 0) {
    return 1;
  }
  if (ShouldRejectEmptyAndMultipleDirectText() != 0) {
    return 1;
  }
  if (ShouldAllowLargeNonInteractiveInput() != 0) {
    return 1;
  }
  if (ShouldAllowLargeDirectTextWithoutConfirmation() != 0) {
    return 1;
  }

  return 0;
}
