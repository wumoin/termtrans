/**
 * @file test_openai_compatible_translator.cpp
 * @brief 测试 OpenAI-compatible provider 请求和响应解析。
 */

#include "translate/openai_compatible_translator.h"

#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

/**
 * @brief 记录单个测试断言失败信息。
 */
bool Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    return false;
  }

  return true;
}

/**
 * @brief 测试用流式输出 sink。
 */
class CapturingSink : public termtrans::translate::TranslationStreamSink {
 public:
  bool Write(std::string_view text) override {
    text_ += text;
    return !cancel_on_write_;
  }

  void set_cancel_on_write(bool cancel_on_write) {
    cancel_on_write_ = cancel_on_write;
  }

  const std::string& text() const { return text_; }

 private:
  std::string text_;
  bool cancel_on_write_ = false;
};

/**
 * @brief 测试用 fake HTTP client。
 */
class FakeHttpClient : public termtrans::translate::HttpClient {
 public:
  termtrans::translate::HttpResponse Post(
      const termtrans::translate::HttpRequest& request) override {
    last_request = request;
    return response;
  }

  termtrans::translate::HttpResponse PostStream(
      const termtrans::translate::HttpRequest& request,
      const termtrans::translate::HttpStreamCallback& callback) override {
    last_request = request;
    for (const std::string& chunk : stream_chunks) {
      if (!callback(chunk)) {
        return termtrans::translate::HttpResponse{
            .is_ok = false,
            .error_message = "request cancelled",
        };
      }
    }
    return response;
  }

  termtrans::translate::HttpRequest last_request;
  termtrans::translate::HttpResponse response;
  std::vector<std::string> stream_chunks;
};

/**
 * @brief 构造合法模型配置。
 */
termtrans::config::ModelProfile MakeModelProfile() {
  return termtrans::config::ModelProfile{
      .name = "deepseek",
      .provider_type = "openai-compatible",
      .base_url = "https://api.deepseek.com",
      .model_id = "deepseek-v4-flash",
      .api_key = "test-token-secret",
      .timeout_seconds = 12,
  };
}

/**
 * @brief 构造测试翻译请求。
 */
termtrans::translate::TranslateRequest MakeRequest(bool stream) {
  return termtrans::translate::TranslateRequest{
      .target_language = "zh-CN",
      .system_prompt = "只输出译文。",
      .source_text = "hello world",
      .recent_translated_text = "前文译文",
      .stream = stream,
  };
}

/**
 * @brief 验证非流式响应解析。
 */
int ShouldParseNonStreamResponse() {
  FakeHttpClient http_client;
  http_client.response = termtrans::translate::HttpResponse{
      .is_ok = true,
      .status_code = 200,
      .body = R"({"choices":[{"message":{"content":"你好，世界"}}]})",
  };
  termtrans::translate::OpenAICompatibleTranslator translator(
      MakeModelProfile(), &http_client);

  const termtrans::translate::TranslateResult result =
      translator.Translate(MakeRequest(false), nullptr);

  return Expect(result.is_ok, "非流式响应应解析成功") &&
                 Expect(result.translated_text == "你好，世界",
                        "应提取 message.content") &&
                 Expect(http_client.last_request.url ==
                            "https://api.deepseek.com/chat/completions",
                        "应拼接 chat completions endpoint") &&
                 Expect(http_client.last_request.body.find("test-token-secret") ==
                            std::string::npos,
                        "请求 body 不应包含 API key")
             ? 0
             : 1;
}

/**
 * @brief 验证流式 SSE 响应解析。
 */
int ShouldParseStreamResponse() {
  FakeHttpClient http_client;
  http_client.response = termtrans::translate::HttpResponse{
      .is_ok = true,
      .status_code = 200,
  };
  http_client.stream_chunks = {
      "data: {\"choices\":[{\"delta\":{\"content\":\"你\"}}]}\n",
      "data: {\"choices\":[{\"delta\":{\"content\":\"好\"}}]}\n",
      "data: [DONE]\n",
  };
  CapturingSink sink;
  termtrans::translate::OpenAICompatibleTranslator translator(
      MakeModelProfile(), &http_client);

  const termtrans::translate::TranslateResult result =
      translator.Translate(MakeRequest(true), &sink);

  return Expect(result.is_ok, "流式响应应解析成功") &&
                 Expect(result.translated_text == "你好",
                        "流式结果应聚合完整译文") &&
                 Expect(sink.text() == "你好",
                        "流式增量应写入 sink")
             ? 0
             : 1;
}

/**
 * @brief 验证流式响应可处理跨 chunk 的一行数据。
 */
int ShouldParseSplitStreamLine() {
  FakeHttpClient http_client;
  http_client.response = termtrans::translate::HttpResponse{
      .is_ok = true,
      .status_code = 200,
  };
  http_client.stream_chunks = {
      "data: {\"choices\":[{\"delta\":{\"content\":\"hel",
      "lo\"}}]}\n",
      "data: [DONE]\n",
  };
  CapturingSink sink;
  termtrans::translate::OpenAICompatibleTranslator translator(
      MakeModelProfile(), &http_client);

  const termtrans::translate::TranslateResult result =
      translator.Translate(MakeRequest(true), &sink);

  return Expect(result.is_ok, "跨 chunk SSE 行应解析成功") &&
                 Expect(result.translated_text == "hello",
                        "跨 chunk delta 应正确拼接")
             ? 0
             : 1;
}

/**
 * @brief 验证 HTTP 错误会返回 provider 错误。
 */
int ShouldReportHttpErrors() {
  FakeHttpClient http_client;
  http_client.response = termtrans::translate::HttpResponse{
      .is_ok = false,
      .status_code = 401,
      .error_message = "provider returned HTTP 401",
  };
  termtrans::translate::OpenAICompatibleTranslator translator(
      MakeModelProfile(), &http_client);

  const termtrans::translate::TranslateResult result =
      translator.Translate(MakeRequest(false), nullptr);

  return Expect(!result.is_ok, "HTTP 错误不应成功") &&
                 Expect(result.error_message.find("401") != std::string::npos,
                        "错误文本应包含 HTTP 状态") &&
                 Expect(result.error_message.find("test-token-secret") ==
                            std::string::npos,
                        "错误文本不应包含 API key")
             ? 0
             : 1;
}

/**
 * @brief 验证畸形 JSON 会返回解析错误。
 */
int ShouldRejectMalformedJson() {
  FakeHttpClient http_client;
  http_client.response = termtrans::translate::HttpResponse{
      .is_ok = true,
      .status_code = 200,
      .body = R"({"bad":true})",
  };
  termtrans::translate::OpenAICompatibleTranslator translator(
      MakeModelProfile(), &http_client);

  const termtrans::translate::TranslateResult result =
      translator.Translate(MakeRequest(false), nullptr);

  return Expect(!result.is_ok, "畸形 provider JSON 不应成功") &&
                 Expect(result.error_message.find("parse") != std::string::npos,
                        "应返回解析错误摘要")
             ? 0
             : 1;
}

/**
 * @brief 验证 sink 取消会停止流式翻译。
 */
int ShouldPropagateStreamCancellation() {
  FakeHttpClient http_client;
  http_client.stream_chunks = {
      "data: {\"choices\":[{\"delta\":{\"content\":\"hello\"}}]}\n",
  };
  CapturingSink sink;
  sink.set_cancel_on_write(true);
  termtrans::translate::OpenAICompatibleTranslator translator(
      MakeModelProfile(), &http_client);

  const termtrans::translate::TranslateResult result =
      translator.Translate(MakeRequest(true), &sink);

  return Expect(!result.is_ok, "sink 取消后不应成功") &&
                 Expect(result.is_cancelled, "sink 取消应标记 cancelled")
             ? 0
             : 1;
}

}  // namespace

/**
 * @brief 运行 OpenAI-compatible provider 测试。
 */
int main() {
  if (ShouldParseNonStreamResponse() != 0) {
    return 1;
  }
  if (ShouldParseStreamResponse() != 0) {
    return 1;
  }
  if (ShouldParseSplitStreamLine() != 0) {
    return 1;
  }
  if (ShouldReportHttpErrors() != 0) {
    return 1;
  }
  if (ShouldRejectMalformedJson() != 0) {
    return 1;
  }
  if (ShouldPropagateStreamCancellation() != 0) {
    return 1;
  }

  return 0;
}
