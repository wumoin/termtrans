/**
 * @file translation_session.cpp
 * @brief 实现统一分块翻译会话。
 */

#include "translate/translation_session.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace termtrans::translate {
namespace {

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
 * @brief 将起始位置向前推进到 UTF-8 字符边界。
 *
 * @param text 已完成译文。
 * @param begin 原始起始位置。
 * @return 位于字符边界的起始位置。
 */
std::size_t MoveBeginToUtf8Boundary(const std::string& text,
                                    std::size_t begin) {
  while (begin < text.size() &&
         IsUtf8ContinuationByte(static_cast<unsigned char>(text[begin]))) {
    ++begin;
  }

  return begin;
}

/**
 * @brief 裁剪最近译文上下文。
 *
 * 上下文只用于帮助后续 chunk 保持术语、语气和格式一致，不参与历史 key
 * 或持久化。裁剪时会移动到 UTF-8 字符边界，避免后续 provider JSON
 * 请求体包含非法 UTF-8。
 *
 * @param text 已完成译文。
 * @param max_bytes 保留上限，单位字节。
 * @return 最近译文片段。
 */
std::string RecentSuffix(const std::string& text, std::size_t max_bytes) {
  if (text.size() <= max_bytes) {
    return text;
  }

  const std::size_t begin =
      MoveBeginToUtf8Boundary(text, text.size() - max_bytes);
  return text.substr(begin);
}

}  // namespace

/**
 * @brief 创建翻译会话。
 *
 * @param translator provider 翻译器，不能为空且生命周期长于本对象。
 * @param chunker 输入分块器。
 * @param context_bytes 最近译文上下文保留字节数。
 */
TranslationSession::TranslationSession(Translator* translator,
                                       Chunker chunker,
                                       std::size_t context_bytes)
    : translator_(translator),
      chunker_(std::move(chunker)),
      context_bytes_(context_bytes) {}

/**
 * @brief 执行完整输入翻译。
 *
 * @param request 会话请求。
 * @param sink 输出 sink；流式和非流式模式都用于最终输出。
 * @return 会话结果。
 */
TranslationSessionResult TranslationSession::Run(
    const TranslationSessionRequest& request,
    TranslationStreamSink* sink) {
  if (translator_ == nullptr) {
    return TranslationSessionResult{
        .is_ok = false,
        .error_message = "translator is not configured",
    };
  }

  const std::vector<std::string> chunks = chunker_.Split(request.input_text);
  if (chunks.empty()) {
    return TranslationSessionResult{
        .is_ok = false,
        .error_message = "input is empty",
    };
  }

  TranslationSessionResult session_result;
  std::string recent_translated_text;
  for (const std::string& chunk : chunks) {
    TranslateRequest chunk_request{
        .target_language = request.target_language,
        .system_prompt = request.system_prompt,
        .source_text = chunk,
        .recent_translated_text = recent_translated_text,
        .stream = request.stream,
    };
    TranslateResult chunk_result = translator_->Translate(chunk_request, sink);
    if (!chunk_result.is_ok) {
      return TranslationSessionResult{
          .is_ok = false,
          .is_cancelled = chunk_result.is_cancelled,
          .translated_text = session_result.translated_text,
          .error_message = chunk_result.error_message,
      };
    }

    session_result.translated_text += chunk_result.translated_text;
    recent_translated_text =
        RecentSuffix(session_result.translated_text, context_bytes_);
  }

  if (!request.stream && sink != nullptr &&
      !sink->Write(session_result.translated_text)) {
    return TranslationSessionResult{
        .is_ok = false,
        .is_cancelled = true,
        .translated_text = session_result.translated_text,
        .error_message = "output was cancelled",
    };
  }

  session_result.is_ok = true;
  return session_result;
}

}  // namespace termtrans::translate
