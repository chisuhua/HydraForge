// tests/test_helpers/http_mock_server.h
// HttpMockServer RAII helper — 封装 httplib::Server 启动/关闭模板。
// 用途: 测试中快速 mock 一个 HTTP server, 析构期自动 stop + join, 防止后台 thread 泄漏。
// 作者: HydraForge team, Sprint 20 (extract-http-mock-server-helper)
// 日期: 2026-07-01
#pragma once

#include <httplib.h>
#include <chrono>
#include <stdexcept>
#include <thread>

namespace agenticdsl::test {

/// @brief RAII helper: 启动 httplib mock server, 绑定 127.0.0.1 随机端口, 析构期自动停止。
///
/// 用法:
///   HttpMockServer mock;
///   mock.server().Post("/path", [](const httplib::Request& req, httplib::Response& res) {
///     res.set_content("...", "application/json");
///   });
///   auto url = "http://127.0.0.1:" + std::to_string(mock.port());
///   // ... 测试逻辑 ...
///   // mock 离开 scope 时自动 stop + join
class HttpMockServer {
 public:
  /// @brief 启动 server, 绑定 127.0.0.1 任意可用端口, 启动后台 thread, sleep 100ms 等启动。
  /// @throws std::runtime_error 端口绑定失败时抛异常。
  explicit HttpMockServer() {
    port_ = server_.bind_to_any_port("127.0.0.1");
    if (port_ <= 0) {
      throw std::runtime_error("failed to bind httplib mock server");
    }
    server_thread_ = std::thread([this]() { server_.listen_after_bind(); });
    // 等待 server 完全启动再返回
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  /// @brief 析构: 自动停止 server + join 后台 thread, 0 server thread 泄漏。
  ~HttpMockServer() {
    server_.stop();
    if (server_thread_.joinable()) {
      server_thread_.join();
    }
  }

  // 禁止拷贝 (server 句柄不可复制)
  HttpMockServer(const HttpMockServer&) = delete;
  HttpMockServer& operator=(const HttpMockServer&) = delete;
  HttpMockServer(HttpMockServer&&) = delete;
  HttpMockServer& operator=(HttpMockServer&&) = delete;

  /// @brief 获取绑定的端口号。
  int port() const { return port_; }

  /// @brief 获取 server 引用, 用于注册 handler (e.g. mock.server().Post(...)).
  httplib::Server& server() { return server_; }

 private:
  httplib::Server server_;
  int port_;
  std::thread server_thread_;
};

}  // namespace agenticdsl::test
