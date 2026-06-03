/**
 * @file curl_http_client.cpp
 * @brief 实现基于 libcurl 的 HTTP 客户端。
 */

#include "translate/curl_http_client.h"

#include <curl/curl.h>

#include <map>
#include <string>
#include <string_view>

namespace termtrans::translate {
namespace {

/**
 * @brief 在作用域内初始化 libcurl 全局状态。
 *
 * libcurl 要求进程内先完成全局初始化。该类把初始化边界限制在 HTTP
 * 客户端实现内部，避免 main() 或 Application 暴露 provider 传输细节。
 *
 * @note 该类不是线程安全初始化屏障；第一版 CLI 单进程串行调用。
 */
class CurlGlobalGuard {
 public:
  CurlGlobalGuard() : code_(curl_global_init(CURL_GLOBAL_DEFAULT)) {}

  ~CurlGlobalGuard() {
    if (code_ == CURLE_OK) {
      curl_global_cleanup();
    }
  }

  bool IsOk() const { return code_ == CURLE_OK; }

 private:
  CURLcode code_ = CURLE_FAILED_INIT;  // 全局初始化结果。
};

/**
 * @brief 普通响应写入上下文。
 */
struct WriteContext {
  std::string* body = nullptr;  // 响应体目标，不能为空。
};

/**
 * @brief 流式响应写入上下文。
 */
struct StreamContext {
  const HttpStreamCallback* callback = nullptr;  // 不拥有；调用期间有效。
  bool is_cancelled = false;  // callback 返回 false 后置位。
};

/**
 * @brief 释放 curl_slist 的 RAII 包装。
 */
class CurlHeaderList {
 public:
  CurlHeaderList() = default;
  CurlHeaderList(const CurlHeaderList&) = delete;
  CurlHeaderList& operator=(const CurlHeaderList&) = delete;

  ~CurlHeaderList() {
    if (headers_ != nullptr) {
      curl_slist_free_all(headers_);
    }
  }

  /**
   * @brief 添加一个 HTTP 请求头。
   *
   * @param name 请求头名。
   * @param value 请求头值。
   * @return 添加成功时返回 true。
   */
  bool Append(std::string_view name, std::string_view value) {
    std::string header = std::string(name) + ": " + std::string(value);
    curl_slist* next = curl_slist_append(headers_, header.c_str());
    if (next == nullptr) {
      return false;
    }

    headers_ = next;
    return true;
  }

  curl_slist* get() const { return headers_; }

 private:
  curl_slist* headers_ = nullptr;  // libcurl 请求头链表。
};

/**
 * @brief 追加普通响应体。
 */
std::size_t WriteBodyCallback(char* ptr,
                              std::size_t size,
                              std::size_t nmemb,
                              void* userdata) {
  WriteContext* context = static_cast<WriteContext*>(userdata);
  const std::size_t byte_count = size * nmemb;
  context->body->append(ptr, byte_count);
  return byte_count;
}

/**
 * @brief 转发流式响应字节。
 */
std::size_t WriteStreamCallback(char* ptr,
                                std::size_t size,
                                std::size_t nmemb,
                                void* userdata) {
  StreamContext* context = static_cast<StreamContext*>(userdata);
  const std::size_t byte_count = size * nmemb;
  if (context->callback == nullptr ||
      !(*context->callback)(std::string_view(ptr, byte_count))) {
    context->is_cancelled = true;
    return 0;
  }

  return byte_count;
}

/**
 * @brief 将请求头写入 libcurl 链表。
 *
 * @param request HTTP 请求参数。
 * @param headers 输出请求头链表，不能为空。
 * @return 成功时返回 true。
 */
bool BuildHeaders(const HttpRequest& request, CurlHeaderList* headers) {
  for (const auto& [name, value] : request.headers) {
    if (!headers->Append(name, value)) {
      return false;
    }
  }

  return true;
}

/**
 * @brief 给 CURL easy handle 设置所有请求通用参数。
 *
 * @param curl easy handle。
 * @param request HTTP 请求参数。
 * @param headers 请求头链表。
 */
void SetCommonOptions(CURL* curl,
                      const HttpRequest& request,
                      const CurlHeaderList& headers) {
  curl_easy_setopt(curl, CURLOPT_URL, request.url.c_str());
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,
                   static_cast<long>(request.body.size()));
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.get());
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, request.timeout_seconds);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
}

/**
 * @brief 根据 libcurl 执行结果构造响应。
 *
 * @param curl easy handle。
 * @param code libcurl 请求结果码。
 * @param body 已收集响应体。
 * @param is_cancelled 是否由上游 callback 主动取消。
 * @return HTTP 响应结果。
 */
HttpResponse MakeResponse(CURL* curl,
                          CURLcode code,
                          std::string body,
                          bool is_cancelled) {
  long status_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);

  if (is_cancelled) {
    return HttpResponse{
        .is_ok = false,
        .status_code = status_code,
        .body = std::move(body),
        .error_message = "request cancelled",
    };
  }

  if (code != CURLE_OK) {
    return HttpResponse{
        .is_ok = false,
        .status_code = status_code,
        .body = std::move(body),
        .error_message = curl_easy_strerror(code),
    };
  }

  if (status_code < 200 || status_code >= 300) {
    return HttpResponse{
        .is_ok = false,
        .status_code = status_code,
        .body = std::move(body),
        .error_message = "provider returned HTTP " +
                         std::to_string(status_code),
    };
  }

  return HttpResponse{
      .is_ok = true,
      .status_code = status_code,
      .body = std::move(body),
  };
}

}  // namespace

/**
 * @brief 执行一次普通 HTTP POST。
 *
 * @param request 请求参数。
 * @return HTTP 响应结果。
 */
HttpResponse CurlHttpClient::Post(const HttpRequest& request) {
  CurlGlobalGuard global_guard;
  if (!global_guard.IsOk()) {
    return HttpResponse{.error_message = "failed to initialize curl"};
  }

  CURL* curl = curl_easy_init();
  if (curl == nullptr) {
    return HttpResponse{.error_message = "failed to create curl handle"};
  }

  CurlHeaderList headers;
  if (!BuildHeaders(request, &headers)) {
    curl_easy_cleanup(curl);
    return HttpResponse{.error_message = "failed to build HTTP headers"};
  }

  std::string body;
  WriteContext context{.body = &body};
  SetCommonOptions(curl, request, headers);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteBodyCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &context);

  const CURLcode code = curl_easy_perform(curl);
  HttpResponse response = MakeResponse(curl, code, std::move(body), false);
  curl_easy_cleanup(curl);
  return response;
}

/**
 * @brief 执行一次流式 HTTP POST。
 *
 * @param request 请求参数。
 * @param callback 响应字节回调；返回 false 时取消读取。
 * @return HTTP 响应结果。
 */
HttpResponse CurlHttpClient::PostStream(const HttpRequest& request,
                                        const HttpStreamCallback& callback) {
  CurlGlobalGuard global_guard;
  if (!global_guard.IsOk()) {
    return HttpResponse{.error_message = "failed to initialize curl"};
  }

  CURL* curl = curl_easy_init();
  if (curl == nullptr) {
    return HttpResponse{.error_message = "failed to create curl handle"};
  }

  CurlHeaderList headers;
  if (!BuildHeaders(request, &headers)) {
    curl_easy_cleanup(curl);
    return HttpResponse{.error_message = "failed to build HTTP headers"};
  }

  StreamContext context{.callback = &callback};
  SetCommonOptions(curl, request, headers);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteStreamCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &context);

  const CURLcode code = curl_easy_perform(curl);
  HttpResponse response =
      MakeResponse(curl, code, "", context.is_cancelled);
  curl_easy_cleanup(curl);
  return response;
}

}  // namespace termtrans::translate
