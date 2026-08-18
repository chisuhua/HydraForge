// src/common/env/docker_backend.cpp
// 功能描述：DockerBackend 实施 — cpp-httplib + Docker REST API (ADR-0075 D3)
//          双模式: exec into existing / ephemeral container 生命周期
// 适配说明：libcurl 未 vendor → cpp-httplib (AF_UNIX client 支持 docker.sock)
// 作者：from-roadmap-phase-6c-execution-envbackend change
// 最后修改日期：2026-08-18
#include "agenticdsl/env/docker_backend.h"

#include "agenticdsl/contract/event_builder.h"
#include "common/env/sha256_util.h"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>

namespace agenticdsl {

namespace {

/// @brief 构造 HTTP client: unix socket 路径 (含 '/') → AF_UNIX; 否则 TCP host:port
std::unique_ptr<httplib::Client> make_docker_client(const std::string& host) {
  if (host.find('/') != std::string::npos) {
    auto cli = std::make_unique<httplib::Client>(host, 0);
    cli->set_address_family(AF_UNIX);
    return cli;
  }
  return std::make_unique<httplib::Client>("http://" + host);
}

/// @brief Docker 非 TTY 多路复用流 demux: [stream(1), 0,0,0, size(4 BE)] + payload
/// stream: 1=stdout, 2=stderr; 头部不像 multiplex 时按 raw stdout 处理
void demux_docker_stream(const std::string& body, std::string& out_stdout,
                         std::string& out_stderr) {
  const bool looks_mux =
      body.size() >= 8 &&
      (body[0] == 1 || body[0] == 2) && body[1] == 0 && body[2] == 0 &&
      body[3] == 0;
  if (!looks_mux) {
    out_stdout += body;
    return;
  }
  size_t pos = 0;
  while (pos + 8 <= body.size()) {
    const uint8_t stream = static_cast<uint8_t>(body[pos]);
    const uint32_t size =
        (static_cast<uint32_t>(static_cast<uint8_t>(body[pos + 4])) << 24) |
        (static_cast<uint32_t>(static_cast<uint8_t>(body[pos + 5])) << 16) |
        (static_cast<uint32_t>(static_cast<uint8_t>(body[pos + 6])) << 8) |
        static_cast<uint32_t>(static_cast<uint8_t>(body[pos + 7]));
    if (pos + 8 + size > body.size()) break;
    if (stream == 1) {
      out_stdout.append(body, pos + 8, size);
    } else if (stream == 2) {
      out_stderr.append(body, pos + 8, size);
    }
    pos += 8 + size;
  }
}

std::atomic<uint64_t> g_container_seq{0};

}  // namespace

bool docker_daemon_reachable(const std::string& docker_host) {
  auto cli = make_docker_client(docker_host);
  cli->set_connection_timeout(2, 0);
  cli->set_read_timeout(2, 0);
  auto res = cli->Get("/_ping");
  return res && res->status == 200;
}

DockerBackend::DockerBackend(DockerBackendConfig cfg) : cfg_(std::move(cfg)) {}

BackendCapabilities DockerBackend::capabilities() const {
  return BackendCapabilities{/*supports_isolation=*/true,
                             /*supports_persistent_fs=*/!cfg_.container_id.empty(),
                             /*max_concurrent_execs=*/8};
}

ExecResult DockerBackend::exec(const ExecRequest& req,
                               const ExecOptions& opts) const {
  using Clock = std::chrono::steady_clock;
  const auto t0 = Clock::now();
  ExecResult result;

  // D-7: privileged mode 一律拒绝, 在任何 HTTP 调用之前
  if (cfg_.privileged) {
    result.error_code = BackendErrorCode::SecurityViolation;
    result.stderr_buf = "Privileged mode is forbidden (ADR-0075 D-7)";
    return result;
  }

  const std::string spec =
      "docker:" + (cfg_.container_id.empty() ? cfg_.image : cfg_.container_id);
  const std::string cmd_hash = sha256_hex(req.cmd);
  if (cfg_.bus) {
    cfg_.bus->emit(EventBuilder("env.backend.exec.start")
                       .args({{"backend_spec", spec}, {"cmd_hash", cmd_hash}})
                       .build());
  }

  auto cli = make_docker_client(cfg_.docker_host);
  cli->set_connection_timeout(5, 0);
  const auto read_sec = static_cast<long>(opts.timeout_ms / 1000) + 5;
  cli->set_read_timeout(read_sec, 0);

  // daemon 可达性
  {
    auto ping = cli->Get("/_ping");
    if (!ping || ping->status != 200) {
      result.error_code = BackendErrorCode::Unavailable;
      result.stderr_buf = "docker daemon unreachable: " + cfg_.docker_host;
      result.duration_ms = static_cast<uint64_t>(
          std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t0)
              .count());
      return result;
    }
  }

  // cmd / env 数组
  nlohmann::json cmd_array = nlohmann::json::array();
  cmd_array.push_back(req.cmd);
  for (const auto& a : req.args) cmd_array.push_back(a);
  nlohmann::json env_array = nlohmann::json::array();
  for (const auto& [k, v] : opts.env) env_array.push_back(k + "=" + v);

  bool http_ok = true;
  std::string http_error;

  if (!cfg_.container_id.empty()) {
    // ===== mode (a): exec into existing container =====
    const nlohmann::json exec_body = {{"AttachStdout", true},
                                      {"AttachStderr", opts.capture_stderr},
                                      {"Cmd", cmd_array},
                                      {"Env", env_array}};
    auto res = cli->Post("/containers/" + cfg_.container_id + "/exec",
                         exec_body.dump(), "application/json");
    if (!res || res->status != 201) {
      http_ok = false;
      http_error = "exec create failed";
    } else {
      const std::string exec_id =
          nlohmann::json::parse(res->body).value("Id", "");
      const nlohmann::json start_body = {{"Detach", false}, {"Tty", false}};
      auto start_res = cli->Post("/exec/" + exec_id + "/start",
                                 start_body.dump(), "application/json");
      if (!start_res || start_res->status != 200) {
        http_ok = false;
        http_error = "exec start failed";
      } else {
        demux_docker_stream(start_res->body, result.stdout_buf,
                            result.stderr_buf);
        auto inspect = cli->Get("/exec/" + exec_id + "/inspect");
        if (inspect && inspect->status == 200) {
          result.exit_code =
              nlohmann::json::parse(inspect->body).value("ExitCode", -1);
        }
      }
    }
  } else {
    // ===== mode (b): ephemeral container 生命周期 =====
    std::string cid;
    {
      const int64_t seq =
          static_cast<int64_t>(++g_container_seq) ;
      const std::string name =
          "hydraforge-" + cmd_hash.substr(0, 12) + "-" + std::to_string(seq);
      const nlohmann::json create_body = {
          {"Image", cfg_.image},
          {"Cmd", cmd_array},
          {"Env", env_array},
          {"WorkingDir", req.working_dir},
          {"AttachStdout", true},
          {"AttachStderr", opts.capture_stderr},
          {"HostConfig",
           {{"Memory", static_cast<int64_t>(cfg_.max_memory_mb) * 1024 * 1024},
            {"NanoCpus", static_cast<int64_t>(cfg_.max_cpu_cores) * 1000000000},
            {"Privileged", false},
            {"NetworkMode", "bridge"},
            {"Tmpfs", {{"/tmp", "rw,nosuid,nodev,size=64m"}}}}}};
      auto res = cli->Post("/containers/create?name=" + name,
                           create_body.dump(), "application/json");
      if (!res || res->status != 201) {
        http_ok = false;
        http_error = "container create failed";
      } else {
        cid = nlohmann::json::parse(res->body).value("Id", "");
      }
    }

    if (http_ok) {
      auto start_res = cli->Post("/containers/" + cid + "/start", "", "");
      if (!start_res || (start_res->status != 204 && start_res->status != 304)) {
        http_ok = false;
        http_error = "container start failed";
      }
    }
    if (http_ok) {
      auto wait_res = cli->Post("/containers/" + cid + "/wait", "", "");
      if (!wait_res || wait_res->status != 200) {
        // 读超时 → kill 容器, 标记 timed_out
        cli->Post("/containers/" + cid + "/kill", "", "");
        result.timed_out = true;
        result.error_code = BackendErrorCode::Timeout;
      } else {
        result.exit_code =
            nlohmann::json::parse(wait_res->body).value("StatusCode", -1);
      }
    }
    if (http_ok && !result.timed_out) {
      auto logs = cli->Get("/containers/" + cid + "/logs?stdout=1&stderr=1");
      if (logs && logs->status == 200) {
        demux_docker_stream(logs->body, result.stdout_buf, result.stderr_buf);
      }
    }
    if (!cid.empty()) {
      // ephemeral 无残留: 无论成败都删除
      cli->Delete("/containers/" + cid + "?force=true");
    }
  }

  if (!http_ok && result.error_code == BackendErrorCode::Success) {
    result.error_code = BackendErrorCode::Unavailable;
    result.stderr_buf = "docker REST error: " + http_error;
  }

  // 输出截断 (max_output_bytes, 默认 64KB)
  if (result.stdout_buf.size() > opts.max_output_bytes) {
    result.stdout_buf.resize(opts.max_output_bytes);
    result.error_code = BackendErrorCode::OutputTooLarge;
  }
  if (result.stderr_buf.size() > opts.max_output_bytes) {
    result.stderr_buf.resize(opts.max_output_bytes);
    result.error_code = BackendErrorCode::OutputTooLarge;
  }

  result.duration_ms = static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t0)
          .count());

  if (cfg_.bus) {
    cfg_.bus->emit(
        EventBuilder("env.backend.exec.end")
            .args({{"backend_spec", spec},
                   {"cmd_hash", cmd_hash},
                   {"exit_code", result.exit_code},
                   {"timed_out", result.timed_out},
                   {"error_code", backend_error_name(result.error_code)}})
            .latency_ms(result.duration_ms)
            .build());
  }
  return result;
}

}  // namespace agenticdsl
