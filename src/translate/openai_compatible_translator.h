/**
 * @file openai_compatible_translator.h
 * @brief 声明 OpenAI-compatible Chat Completions 翻译器。
 */

#pragma once

#include "config/config.h"
#include "translate/http_client.h"
#include "translate/translator.h"

#include <string>

namespace termtrans::translate {

/**
 * @brief 调用 OpenAI-compatible Chat Completions API 的翻译器。
 *
 * 该类构造 Chat Completions 请求、解析流式 SSE 和非流式 JSON 响应，并
 * 将增量译文写入 TranslationStreamSink。它不读取配置文件、不管理输入
 * 分块，也不在错误文本中包含 API key。
 *
 * @note 该类不持有共享可变状态；HttpClient 的并发限制由具体实现决定。
 */
class OpenAICompatibleTranslator : public Translator {
 public:
  /**
   * @brief 创建 OpenAI-compatible 翻译器。
   *
   * @param model_profile 已解析且合法的本地模型配置。
   * @param http_client HTTP 客户端；调用方必须保证其生命周期长于本对象。
   */
  OpenAICompatibleTranslator(const config::ModelProfile& model_profile,
                             HttpClient* http_client);

  /**
   * @brief 翻译一个输入 chunk。
   *
   * @param request 翻译请求。
   * @param sink 流式输出接收者；非流式模式可为空。
   * @return 当前 chunk 的完整翻译结果。
   */
  TranslateResult Translate(const TranslateRequest& request,
                            TranslationStreamSink* sink) override;

 private:
  config::ModelProfile model_profile_;  // provider 调用参数副本。
  HttpClient* http_client_ = nullptr;  // 不拥有；由应用层或测试持有。
};

}  // namespace termtrans::translate
