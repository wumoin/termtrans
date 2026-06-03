/**
 * @file openai_compatible_translator.cpp
 * @brief 实现 OpenAI-compatible Chat Completions 翻译器。
 */

#include "translate/openai_compatible_translator.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace termtrans::translate {
namespace {

using Json = nlohmann::json;

/**
 * @brief 裁剪字符串两端的 ASCII 空白。
 */
std::string TrimAscii(std::string_view value) {
  std::size_t begin = 0;
  while (begin < value.size() &&
         std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
    ++begin;
  }

  std::size_t end = value.size();
  while (end > begin &&
         std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
    --end;
  }

  return std::string(value.substr(begin, end - begin));
}

/**
 * @brief 拼接 Chat Completions endpoint。
 *
 * 用户可将 base_url 配置成 provider 根地址或 `/v1` 前缀；如果已经包含
 * `/chat/completions`，则直接使用该地址。
 */
std::string BuildChatCompletionsUrl(std::string_view base_url) {
  std::string url = std::string(base_url);
  while (!url.empty() && url.back() == '/') {
    url.pop_back();
  }

  const std::string endpoint = "/chat/completions";
  if (url.size() >= endpoint.size() &&
      url.substr(url.size() - endpoint.size()) == endpoint) {
    return url;
  }

  return url + endpoint;
}

/**
 * @brief 构造用户消息内容。
 *
 * 最近译文上下文是结构化提示的一部分，用于保持术语和格式，不要求用户
 * 自定义 prompt 手写占位符。
 */
std::string BuildUserContent(const TranslateRequest& request) {
  std::string content;
  if (!request.recent_translated_text.empty()) {
    content +=
        "以下是前文已完成译文，仅用于保持术语、语气和格式一致。\n"
        "不要重复输出前文译文。\n"
        "<recent_translated_text>\n";
    content += request.recent_translated_text;
    content += "\n</recent_translated_text>\n\n";
  }

  content += "只翻译当前输入片段。\n<source_text>\n";
  content += request.source_text;
  content += "\n</source_text>";
  return content;
}

/**
 * @brief 构造 Chat Completions JSON 请求体。
 */
std::string BuildRequestBody(const config::ModelProfile& model_profile,
                             const TranslateRequest& request) {
  Json body = {
      {"model", model_profile.model_id},
      {"stream", request.stream},
      {"messages",
       Json::array({
           {{"role", "system"}, {"content", request.system_prompt}},
           {{"role", "user"}, {"content", BuildUserContent(request)}},
       })},
  };

  return body.dump();
}

/**
 * @brief 构造 provider HTTP 请求。
 */
HttpRequest BuildHttpRequest(const config::ModelProfile& model_profile,
                             const TranslateRequest& request) {
  return HttpRequest{
      .url = BuildChatCompletionsUrl(model_profile.base_url),
      .headers =
          {
              {"Authorization", "Bearer " + model_profile.api_key},
              {"Content-Type", "application/json"},
              {"Accept",
               request.stream ? "text/event-stream" : "application/json"},
          },
      .body = BuildRequestBody(model_profile, request),
      .timeout_seconds = model_profile.timeout_seconds,
  };
}

/**
 * @brief 提取非流式响应中的译文。
 */
std::optional<std::string> ParseNonStreamContent(std::string_view body) {
  try {
    const Json json = Json::parse(body);
    if (!json.contains("choices") || !json["choices"].is_array() ||
        json["choices"].empty()) {
      return std::nullopt;
    }

    const Json& message = json["choices"][0]["message"];
    if (!message.is_object() || !message.contains("content") ||
        !message["content"].is_string()) {
      return std::nullopt;
    }

    return message["content"].get<std::string>();
  } catch (const Json::exception&) {
    return std::nullopt;
  }
}

/**
 * @brief 提取一个 SSE data JSON 中的 delta.content。
 */
std::optional<std::string> ParseStreamContentDelta(std::string_view data) {
  try {
    const Json json = Json::parse(data);
    if (!json.contains("choices") || !json["choices"].is_array()) {
      return std::nullopt;
    }

    std::string text;
    for (const Json& choice : json["choices"]) {
      if (!choice.contains("delta") || !choice["delta"].is_object()) {
        continue;
      }
      const Json& delta = choice["delta"];
      if (delta.contains("content") && delta["content"].is_string()) {
        text += delta["content"].get<std::string>();
      }
    }

    return text;
  } catch (const Json::exception&) {
    return std::nullopt;
  }
}

/**
 * @brief 处理完整 SSE 行。
 */
bool HandleSseLine(std::string_view line,
                   std::string* translated_text,
                   bool* is_done,
                   bool* has_parse_error,
                   TranslationStreamSink* sink) {
  constexpr std::string_view kDataPrefix = "data:";
  if (line.rfind(kDataPrefix, 0) != 0) {
    return true;
  }

  const std::string data = TrimAscii(line.substr(kDataPrefix.size()));
  if (data.empty()) {
    return true;
  }

  if (data == "[DONE]") {
    *is_done = true;
    return false;
  }

  const std::optional<std::string> delta = ParseStreamContentDelta(data);
  if (!delta.has_value()) {
    *has_parse_error = true;
    return false;
  }

  if (!delta->empty()) {
    translated_text->append(*delta);
    if (sink != nullptr && !sink->Write(*delta)) {
      return false;
    }
  }

  return true;
}

/**
 * @brief 解析一批 SSE 字节。
 */
bool ConsumeSseBytes(std::string_view chunk,
                     std::string* pending,
                     std::string* translated_text,
                     bool* is_done,
                     bool* has_parse_error,
                     TranslationStreamSink* sink) {
  pending->append(chunk);
  std::size_t line_end = pending->find('\n');
  while (line_end != std::string::npos) {
    std::string line = pending->substr(0, line_end);
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }

    pending->erase(0, line_end + 1);
    if (!HandleSseLine(line, translated_text, is_done, has_parse_error, sink)) {
      return false;
    }

    line_end = pending->find('\n');
  }

  return true;
}

}  // namespace

/**
 * @brief 创建 OpenAI-compatible 翻译器。
 */
OpenAICompatibleTranslator::OpenAICompatibleTranslator(
    const config::ModelProfile& model_profile,
    HttpClient* http_client)
    : model_profile_(model_profile), http_client_(http_client) {}

/**
 * @brief 翻译一个输入 chunk。
 */
TranslateResult OpenAICompatibleTranslator::Translate(
    const TranslateRequest& request,
    TranslationStreamSink* sink) {
  if (http_client_ == nullptr) {
    return TranslateResult{
        .is_ok = false,
        .error_message = "HTTP client is not configured",
    };
  }

  const HttpRequest http_request = BuildHttpRequest(model_profile_, request);
  if (!request.stream) {
    const HttpResponse response = http_client_->Post(http_request);
    if (!response.is_ok) {
      return TranslateResult{
          .is_ok = false,
          .error_message = response.error_message,
      };
    }

    const std::optional<std::string> content =
        ParseNonStreamContent(response.body);
    if (!content.has_value()) {
      return TranslateResult{
          .is_ok = false,
          .error_message = "failed to parse provider response",
      };
    }

    return TranslateResult{
        .is_ok = true,
        .translated_text = *content,
    };
  }

  std::string pending;
  std::string translated_text;
  bool is_done = false;
  bool has_parse_error = false;
  bool is_sink_cancelled = false;
  const HttpResponse response = http_client_->PostStream(
      http_request, [&](std::string_view chunk) {
        const bool should_continue =
            ConsumeSseBytes(chunk, &pending, &translated_text, &is_done,
                            &has_parse_error, sink);
        if (!should_continue && !is_done && !has_parse_error) {
          is_sink_cancelled = true;
        }
        return should_continue;
      });

  if (has_parse_error) {
    return TranslateResult{
        .is_ok = false,
        .error_message = "failed to parse provider stream",
    };
  }

  if (is_sink_cancelled ||
      (response.error_message == "request cancelled" && !is_done)) {
    return TranslateResult{
        .is_ok = false,
        .is_cancelled = true,
        .translated_text = translated_text,
        .error_message = "output was cancelled",
    };
  }

  if (!response.is_ok && !is_done) {
    return TranslateResult{
        .is_ok = false,
        .error_message = response.error_message,
    };
  }

  if (!pending.empty()) {
    bool ignored_done = false;
    if (!HandleSseLine(pending, &translated_text, &ignored_done,
                       &has_parse_error, sink) ||
        has_parse_error) {
      return TranslateResult{
          .is_ok = false,
          .error_message = "failed to parse provider stream",
      };
    }
  }

  return TranslateResult{
      .is_ok = true,
      .translated_text = translated_text,
  };
}

}  // namespace termtrans::translate
