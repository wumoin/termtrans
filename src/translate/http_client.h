/**
 * @file http_client.h
 * @brief 声明 provider 使用的 HTTP 客户端抽象。
 */

#pragma once

#include <functional>
#include <map>
#include <string>
#include <string_view>

namespace termtrans::translate {

/**
 * @brief HTTP POST 请求参数。
 *
 * 该结构只承载 provider 请求所需的稳定字段，不负责 JSON 构造、认证头
 * 脱敏或响应解析。
 */
struct HttpRequest {
  std::string url;  // 完整请求 URL。
  std::map<std::string, std::string> headers;  // HTTP 请求头。
  std::string body;  // 请求 body。
  // 请求超时时间，单位秒；0 表示不设置整体请求超时上限。
  int timeout_seconds = 60;
};

/**
 * @brief HTTP 响应结果。
 */
struct HttpResponse {
  bool is_ok = false;  // HTTP 请求是否成功完成并返回 2xx 状态。
  long status_code = 0;  // provider 返回的 HTTP 状态码，网络失败时为 0。
  std::string body;  // 非流式响应体或聚合后的错误响应体。
  std::string error_message;  // 网络层错误或 HTTP 错误摘要。
};

/**
 * @brief 接收 HTTP 流式响应字节的回调。
 *
 * 返回 false 表示调用方已经取消读取，HTTP 客户端应尽快中断请求。
 */
using HttpStreamCallback = std::function<bool(std::string_view chunk)>;

/**
 * @brief provider 使用的 HTTP 客户端接口。
 *
 * 生产实现通过 libcurl 调用真实网络；测试实现可返回本地 fixture，从而
 * 覆盖 provider 解析逻辑而不访问外部服务。
 *
 * @note 实现类自行声明线程安全性。
 */
class HttpClient {
 public:
  virtual ~HttpClient() = default;

  /**
   * @brief 执行一次普通 HTTP POST。
   *
   * @param request 请求参数。
   * @return HTTP 响应结果。
   */
  virtual HttpResponse Post(const HttpRequest& request) = 0;

  /**
   * @brief 执行一次流式 HTTP POST。
   *
   * @param request 请求参数。
   * @param callback 响应字节回调；返回 false 时取消读取。
   * @return HTTP 响应结果。
   */
  virtual HttpResponse PostStream(const HttpRequest& request,
                                  const HttpStreamCallback& callback) = 0;
};

}  // namespace termtrans::translate
