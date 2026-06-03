/**
 * @file curl_http_client.h
 * @brief 声明基于 libcurl 的 HTTP 客户端。
 */

#pragma once

#include "translate/http_client.h"

namespace termtrans::translate {

/**
 * @brief 使用 libcurl 执行 provider HTTP 请求。
 *
 * 该类只负责 HTTP 传输，不构造 provider JSON，也不解析响应内容。libcurl
 * 的全局初始化在每次调用中通过局部 RAII 对象完成，避免 main() 持有网络
 * 生命周期细节。
 *
 * @note 该类不保存共享可变状态，可并发创建多个实例。
 */
class CurlHttpClient : public HttpClient {
 public:
  /**
   * @brief 执行一次普通 HTTP POST。
   *
   * @param request 请求参数。
   * @return HTTP 响应结果。
   */
  HttpResponse Post(const HttpRequest& request) override;

  /**
   * @brief 执行一次流式 HTTP POST。
   *
   * @param request 请求参数。
   * @param callback 响应字节回调；返回 false 时取消读取。
   * @return HTTP 响应结果。
   */
  HttpResponse PostStream(const HttpRequest& request,
                          const HttpStreamCallback& callback) override;
};

}  // namespace termtrans::translate
