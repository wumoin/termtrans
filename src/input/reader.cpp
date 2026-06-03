/**
 * @file reader.cpp
 * @brief 实现翻译输入读取接口。
 */

#include "input/reader.h"

#include <istream>
#include <sstream>
#include <string>
#include <vector>

namespace termtrans::input {
namespace {

/**
 * @brief 从输入流读取完整文本。
 *
 * @param input 输入流。
 * @return 读取到的完整文本。
 */
std::string ReadAll(std::istream& input) {
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

}  // namespace

/**
 * @brief 读取 stdin 或一个直接文本位置参数。
 */
ReadInputResult ReadInput(const std::vector<std::string>& positionals,
                          std::istream& stdin_stream,
                          const ReadInputOptions& options) {
  if (!options.is_stdin_interactive) {
    std::string text = ReadAll(stdin_stream);
    if (stdin_stream.bad()) {
      return ReadInputResult{
          .is_ok = false,
          .error_kind = ReadInputErrorKind::kReadFailed,
      };
    }

    if (text.empty()) {
      return ReadInputResult{
          .is_ok = false,
          .error_kind = ReadInputErrorKind::kEmptyInput,
      };
    }

    return ReadInputResult{.is_ok = true, .text = std::move(text)};
  }

  if (positionals.empty()) {
    return ReadInputResult{
        .is_ok = false,
        .error_kind = ReadInputErrorKind::kEmptyInput,
    };
  }

  if (positionals.size() != 1) {
    return ReadInputResult{
        .is_ok = false,
        .error_kind = ReadInputErrorKind::kInvalidUsage,
    };
  }

  std::string text = positionals.front();
  if (text.empty()) {
    return ReadInputResult{
        .is_ok = false,
        .error_kind = ReadInputErrorKind::kEmptyInput,
    };
  }

  return ReadInputResult{.is_ok = true, .text = std::move(text)};
}

}  // namespace termtrans::input
