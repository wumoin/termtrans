/**
 * @file translation_session.h
 * @brief 声明统一分块翻译会话。
 */

#pragma once

#include "translate/chunker.h"
#include "translate/translator.h"

#include <cstddef>
#include <string>

namespace termtrans::translate {

/**
 * @brief 一次完整输入翻译会话的配置。
 */
struct TranslationSessionRequest {
  std::string target_language;  // 目标语言代码或自定义语言标识。
  std::string system_prompt;  // 当前目标语言的完整 prompt。
  std::string input_text;  // 完整输入文本。
  bool stream = true;  // 是否流式输出。
};

/**
 * @brief 一次完整输入翻译会话的结果。
 */
struct TranslationSessionResult {
  bool is_ok = false;  // 所有 chunk 是否翻译成功。
  bool is_cancelled = false;  // 是否因下游取消停止。
  std::string translated_text;  // 完整译文。
  std::string error_message;  // 失败原因摘要。
};

/**
 * @brief 统一处理短文本和长文本的翻译会话。
 *
 * TranslationSession 负责分块、逐 chunk 调用 Translator、保存最近译文
 * 片段作为短期上下文，并将译文交给输出 sink。它不读配置、不查历史，也
 * 不决定 provider 类型。
 *
 * @note 该类不是线程安全的。
 */
class TranslationSession {
 public:
  /**
   * @brief 创建翻译会话。
   *
   * @param translator provider 翻译器，不能为空且生命周期长于本对象。
   * @param chunker 输入分块器。
   * @param context_bytes 最近译文上下文保留字节数。
   */
  TranslationSession(Translator* translator,
                     Chunker chunker = Chunker(),
                     std::size_t context_bytes = 2000);

  /**
   * @brief 执行完整输入翻译。
   *
   * @param request 会话请求。
   * @param sink 输出 sink；流式和非流式模式都用于最终输出。
   * @return 会话结果。
   */
  TranslationSessionResult Run(const TranslationSessionRequest& request,
                               TranslationStreamSink* sink);

 private:
  Translator* translator_ = nullptr;  // 不拥有；由调用方持有。
  Chunker chunker_;  // 输入分块策略。
  std::size_t context_bytes_ = 2000;  // 最近译文上下文保留上限，单位字节。
};

}  // namespace termtrans::translate
