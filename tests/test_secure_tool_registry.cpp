// tests/test_secure_tool_registry.cpp
// 文件头注释
// 功能描述：SecureToolRegistry 装饰器单元测试（ADR-0004 §5 验证）
//          覆盖：
//            - 装饰器包装 ToolRegistry 不破坏已有调用
//            - disable_tool 黑名单生效
//            - fs.* 工具走 PathPolicy 拦截
//            - shell.exec 走 ShellGuard 拦截
//            - 透传路径（call_passthrough）跳过检查但保留 disabled 检查
//            - 未注册工具返回 ToolNotRegistered
//            - 线程安全：disable/get_path_policy 在多线程下不崩
// 作者：docs-code-drift-audit-2026-06 change
// 最后修改日期：2026-06-13

#include "catch_amalgamated.hpp"
#include "agenticdsl/tools/secure_tool_registry.h"
#include "common/tools/registry.h"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <thread>
#include <vector>

using namespace agenticdsl;

namespace {

// 注册一个测试用 fs.read 工具
void register_fs_read(ToolRegistry& reg) {
  reg.register_tool("fs.read", agenticdsl::ToolMetadata{"fs.read", "test", "test", agenticdsl::ToolCategory::ReadOnly, agenticdsl::LayerProfile::Workflow}, [](const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
    auto it = args.find("path");
    std::string path = (it != args.end()) ? it->second : "";
    return nlohmann::json{{"path_read", path}, {"content", "mock_content"}};
  });
}

// 注册一个测试用 shell.exec 工具
void register_shell_exec(ToolRegistry& reg) {
  reg.register_tool("shell.exec", agenticdsl::ToolMetadata{"shell.exec", "test", "test", agenticdsl::ToolCategory::ReadOnly, agenticdsl::LayerProfile::Workflow}, [](const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
    auto it = args.find("command");
    std::string cmd = (it != args.end()) ? it->second : "";
    return nlohmann::json{{"executed", cmd}};
  });
}

}  // namespace

TEST_CASE("SecureToolRegistry passthrough for read-only tool", "[secure_tool_registry][stage1]") {
  ToolRegistry base;
  SecureToolRegistry secure(base);

  auto r = secure.call_direct("web_search", {{"query", "hello"}});
  REQUIRE(r.allowed == true);
  REQUIRE(r.payload.contains("results"));
}

TEST_CASE("SecureToolRegistry blocks disabled tool", "[secure_tool_registry][stage1]") {
  ToolRegistry base;
  SecureToolRegistry secure(base);
  secure.disable_tool("web_search");

  auto r = secure.call_direct("web_search", {{"query", "hello"}});
  REQUIRE(r.allowed == false);
  REQUIRE(r.error.code == SecurityError::Code::PermissionDenied);

  // 重新启用
  secure.enable_tool("web_search");
  auto r2 = secure.call_direct("web_search", {{"query", "hello"}});
  REQUIRE(r2.allowed == true);
}

TEST_CASE("SecureToolRegistry blocks fs.read on /etc/passwd", "[secure_tool_registry][stage1]") {
  ToolRegistry base;
  register_fs_read(base);
  SecureToolRegistry secure(base);

  auto r = secure.call_direct("fs.read", {{"path", "/etc/passwd"}});
  REQUIRE(r.allowed == false);
  REQUIRE(r.error.code == SecurityError::Code::PathViolation);
  REQUIRE(r.error.message.find("/etc/passwd") != std::string::npos);
}

TEST_CASE("SecureToolRegistry allows fs.read on workspace path", "[secure_tool_registry][stage1]") {
  ToolRegistry base;
  register_fs_read(base);
  SecureToolRegistry secure(base);

  namespace fs = std::filesystem;
  fs::create_directories("./workspace");
  std::string test_file = "./workspace/test_read.txt";
  {
    std::ofstream of(test_file);
    of << "hello";
  }

  auto r = secure.call_direct("fs.read", {{"path", test_file}});
  REQUIRE(r.allowed == true);
  REQUIRE(r.payload.contains("path_read"));

  fs::remove(test_file);
  fs::remove("./workspace");
}

TEST_CASE("SecureToolRegistry blocks shell.exec with rm -rf", "[secure_tool_registry][stage1]") {
  ToolRegistry base;
  register_shell_exec(base);
  SecureToolRegistry secure(base);

  auto r = secure.call_direct("shell.exec", {{"command", "rm -rf /tmp/build"}});
  REQUIRE(r.allowed == false);
  REQUIRE(r.error.code == SecurityError::Code::DangerousCommand);
}

TEST_CASE("SecureToolRegistry allows safe shell command", "[secure_tool_registry][stage1]") {
  ToolRegistry base;
  register_shell_exec(base);
  SecureToolRegistry secure(base);

  auto r = secure.call_direct("shell.exec", {{"command", "ls -la /tmp"}});
  REQUIRE(r.allowed == true);
  REQUIRE(r.payload.contains("executed"));
}

TEST_CASE("SecureToolRegistry rejects unregistered tool", "[secure_tool_registry][stage1]") {
  ToolRegistry base;
  SecureToolRegistry secure(base);

  auto r = secure.call_direct("nonexistent_tool", {});
  REQUIRE(r.allowed == false);
  REQUIRE(r.error.code == SecurityError::Code::ToolNotRegistered);
}

TEST_CASE("SecureToolRegistry call_passthrough skips path/shell checks", "[secure_tool_registry][stage1]") {
  ToolRegistry base;
  register_fs_read(base);
  SecureToolRegistry secure(base);

  // passthrough 不应被 PathPolicy 拦截（即便路径是 /etc/passwd）
  auto r = secure.call_passthrough("fs.read", {{"path", "/etc/passwd"}});
  REQUIRE(r.allowed == true);
  REQUIRE(r.payload.contains("path_read"));
}

TEST_CASE("SecureToolRegistry call_passthrough respects disabled list", "[secure_tool_registry][stage1]") {
  ToolRegistry base;
  SecureToolRegistry secure(base);
  secure.disable_tool("web_search");

  // disabled 仍生效
  auto r = secure.call_passthrough("web_search", {{"query", "x"}});
  REQUIRE(r.allowed == false);
  REQUIRE(r.error.code == SecurityError::Code::PermissionDenied);
}

TEST_CASE("SecureToolRegistry custom path policy per-tool", "[secure_tool_registry][stage1]") {
  ToolRegistry base;
  register_fs_read(base);
  SecureToolRegistry secure(base);

  // 给 fs.read 自定义策略：禁用 ./secrets/
  PathPolicy custom;
  custom.allowed_prefixes.clear();
  custom.denied_patterns = {std::regex(R"(secrets)")};
  secure.set_path_policy("fs.read", custom);

  namespace fs = std::filesystem;
  fs::create_directories("./secrets");
  std::string secret_file = "./secrets/key.txt";
  {
    std::ofstream of(secret_file);
    of << "secret";
  }

  auto r = secure.call_direct("fs.read", {{"path", secret_file}});
  REQUIRE(r.allowed == false);
  REQUIRE(r.error.code == SecurityError::Code::PathViolation);

  // 全局默认策略不受影响（其他工具未设置时仍用 default）
  auto def = secure.get_default_path_policy();
  REQUIRE_FALSE(def.allowed_prefixes.empty());

  fs::remove(secret_file);
  fs::remove("./secrets");
}

TEST_CASE("SecureToolRegistry thread safety on concurrent calls", "[secure_tool_registry][stage1][thread]") {
  ToolRegistry base;
  register_fs_read(base);
  register_shell_exec(base);
  SecureToolRegistry secure(base);

  // 启动 4 线程 × 100 次调用，验证无 data race（启用 TSAN 即可彻底验证）
  std::vector<std::thread> threads;
  std::atomic<int> allowed_count{0};
  std::atomic<int> denied_count{0};

  for (int t = 0; t < 4; ++t) {
    threads.emplace_back([&secure, &allowed_count, &denied_count, t]() {
      for (int i = 0; i < 100; ++i) {
        if (i % 2 == 0) {
          // 偶数次：安全 fs.read
          auto r = secure.call_direct("fs.read", {{"path", "./workspace/x"}});
          if (r.allowed) allowed_count++;
          else denied_count++;
        } else {
          // 奇数次：危险 shell
          auto r = secure.call_direct("shell.exec", {{"command", "rm -rf /"}});
          if (r.allowed) allowed_count++;
          else denied_count++;
        }
        (void)t;
      }
    });
  }
  for (auto& th : threads) th.join();

  // shell.exec 全数被 ShellGuard 拒绝，fs.read 全部通过
  REQUIRE(denied_count.load() == 200);
  REQUIRE(allowed_count.load() == 200);
}
