/**
 * @file translator.h
 * @brief 声明翻译 provider 的统一接口。
 */

#pragma once

#include <string>

namespace termtrans::translate {

/**
 * @brief 一次 provider chunk 翻译请求。
 */
struct TranslateRequest {
  std::string target_language;  // 目标语言代码或自定义语言标识。
  std::string system_prompt;  // 当前目标语言的完整 prompt。
  std::string source_text;  // 当前 chunk 原文。
  std::string recent_translated_text;  // 前文最近译文片段，可为空。
  bool stream = true;  // 是否请求 provider 流式响应。
};

/**
 * @brief 一次 provider chunk 翻译结果。
 */
struct TranslateResult {
  bool is_ok = false;  // 当前 chunk 是否翻译成功。
  bool is_cancelled = false;  // 是否因用户取消或下游关闭而停止。
  std::string translated_text;  // 当前 chunk 完整译文。
  std::string error_message;  // provider 或解析错误摘要，不得包含 API key。
};

/**
 * @brief 接收 provider 流式译文增量。
 *
 * 返回 false 表示下游已经取消，provider 应停止后续请求或响应读取。
 *
 * @note 实现类自行声明线程安全性。
 */
class TranslationStreamSink {
 public:
  virtual ~TranslationStreamSink() = default;

  /**
   * @brief 写入一段译文增量。
   *
   * @param text provider 返回的译文增量。
   * @return 下游仍可继续接收时返回 true。
   */
  virtual bool Write(std::string_view text) = 0;
};

/**
 * @brief 翻译 provider 接口。
 *
 * Translator 只处理单个 chunk 的 provider 调用和响应解析，不负责分块、
 * 历史记录、stdout 策略或配置文件读取。
 *
 * @note 实现类自行声明线程安全性。
 */
class Translator {
 public:
  virtual ~Translator() = default;

  /**
   * @brief 翻译一个输入 chunk。
   *
   * @param request 翻译请求。
   * @param sink 流式输出接收者；非流式模式仍可为空。
   * @return 当前 chunk 的完整翻译结果。
   */
  virtual TranslateResult Translate(const TranslateRequest& request,
                                    TranslationStreamSink* sink) = 0;
};

}  // namespace termtrans::translate
