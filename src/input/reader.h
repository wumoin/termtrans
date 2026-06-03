/**
 * @file reader.h
 * @brief 声明翻译输入读取接口。
 */

#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

namespace termtrans::input {

/**
 * @brief 输入读取失败类别。
 */
enum class ReadInputErrorKind {
  kNone,          // 未发生错误。
  kEmptyInput,    // stdin 和直接文本都没有有效输入。
  kInvalidUsage,  // 位置参数数量不符合直接文本规则。
  kReadFailed,    // stdin 读取失败。
};

/**
 * @brief 翻译输入读取选项。
 */
struct ReadInputOptions {
  bool is_stdin_interactive = true;  // stdin 是否是交互终端。
};

/**
 * @brief 翻译输入读取结果。
 */
struct ReadInputResult {
  bool is_ok = false;  // 是否成功获得输入文本。
  ReadInputErrorKind error_kind = ReadInputErrorKind::kNone;  // 失败类别。
  std::string text;  // 成功读取到的完整输入文本。
};

/**
 * @brief 读取 stdin 或一个直接文本位置参数。
 *
 * 输入优先级遵循设计文档：stdin 非交互时读取 stdin；否则仅接受一个
 * 直接文本参数。该函数不读取文件路径。
 *
 * @param positionals CLI 解析得到的位置参数。
 * @param stdin_stream stdin 输入流。
 * @param options 输入读取选项。
 * @return 输入读取结果。
 */
ReadInputResult ReadInput(const std::vector<std::string>& positionals,
                          std::istream& stdin_stream,
                          const ReadInputOptions& options);

}  // namespace termtrans::input
