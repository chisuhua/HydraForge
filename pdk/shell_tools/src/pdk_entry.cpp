// PDK Plugin entry — AgenticDSL v1 API (hydraforge namespace for PluginInfo, agenticdsl for IToolRegistry)
// 关联: openspec/changes/2026-07-17-pdk-chat-demo-buildable/

#include <array>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>

#include <nlohmann/json.hpp>

#include <agenticdsl/contract/itool_registry.h>
#include <agenticdsl/plugin/plugin_info.h>

namespace {

inline std::string str_arg(const std::unordered_map<std::string, std::string>& args,
                           const std::string& key, const std::string& default_val = "") {
    auto it = args.find(key);
    return (it != args.end()) ? it->second : default_val;
}

}  // namespace

// --- pdk_plugin_info ---
extern "C" const hydraforge::PluginInfo pdk_plugin_info = {
    hydraforge::CURRENT_ABI_VERSION,                  // abi_version = 2
    "tool.shell",                                     // name[64]
    0, 1, 0,                                          // semver major.minor.patch
    "Shell Tools - shell exec/which/env",             // description[256]
    "shell_exec,shell_which,shell_env",               // capabilities[512]
    ""                                                // dependencies[256]
};

// --- pdk_register_tools ---
extern "C" void pdk_register_tools(::agenticdsl::IToolRegistry& registry) {
    // shell/exec
    registry.register_tool_function(
        "shell/exec",
        ::agenticdsl::ToolMetadata{
            .name = "shell/exec",
            .description = "Execute shell command",
            .domain = "shell",
            .category = ::agenticdsl::ToolCategory::Execute,
            .min_layer = ::agenticdsl::LayerProfile::Workflow,
            .approval = ::agenticdsl::ApprovalPolicy{
                .requires_approval_in_plan = true,
                .requires_approval_in_agent = true,
                .requires_approval_in_yolo = false,
                .force_approval_always = true
            },
            .allowed_layers = {::agenticdsl::LayerProfile::Workflow}
        },
        [](const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
            std::string command = str_arg(args, "command");
            // 健壮的 timeout 解析 (避免 std::stoi 在非数字输入时抛异常)
            int timeout_ms = 30000;
            try {
                timeout_ms = std::stoi(str_arg(args, "timeout_ms", "30000"));
            } catch (...) {
                timeout_ms = 30000;
            }
            if (command.empty()) throw std::runtime_error("command is required");

            // 危险命令黑名单
            static const std::vector<std::string> blacklist = {
                "rm -rf /", "dd if=", ":(){:|:&};:", "mkfs", "fdisk"
            };
            for (const auto& bad : blacklist) {
                if (command.find(bad) != std::string::npos) {
                    throw std::runtime_error("dangerous command blocked: " + bad);
                }
            }

            // fork + exec + pipe
            int pipefd[2];
            if (pipe(pipefd) != 0) {
                throw std::runtime_error("pipe() failed");
            }

            pid_t pid = fork();
            if (pid < 0) {
                close(pipefd[0]); close(pipefd[1]);
                throw std::runtime_error("fork() failed");
            }

            if (pid == 0) {
                // child
                close(pipefd[0]);
                dup2(pipefd[1], STDOUT_FILENO);
                dup2(pipefd[1], STDERR_FILENO);
                close(pipefd[1]);
                execl("/bin/sh", "sh", "-c", command.c_str(), (char*)nullptr);
                _exit(127);
            }

            // parent
            close(pipefd[1]);

            // poll() 读循环 + WNOHANG 子进程检查 + 超时升级 (SIGTERM → 1s grace → SIGKILL)
            // 设计依据: 旧实现 read() 立即 break,waitpid() 阻塞无限,timeout_ms 是 no-op
            std::string output;
            std::array<char, 4096> buf;
            auto deadline = std::chrono::steady_clock::now()
                          + std::chrono::milliseconds(timeout_ms);
            bool timed_out = false;
            int status = 0;
            bool child_reaped = false;

            while (true) {
                pid_t wp = waitpid(pid, &status, WNOHANG);
                if (wp == pid) {
                    child_reaped = true;
                    ssize_t n;
                    while ((n = read(pipefd[0], buf.data(), buf.size())) > 0) {
                        output.append(buf.data(), n);
                    }
                    break;
                }
                if (wp < 0 && errno != EINTR) break;

                auto now = std::chrono::steady_clock::now();
                if (now >= deadline) {
                    timed_out = true;
                    kill(pid, SIGTERM);
                    // 1 秒宽限期后 SIGKILL (防止子进程忽略 SIGTERM)
                    auto grace_deadline = now + std::chrono::seconds(1);
                    while (waitpid(pid, &status, WNOHANG) == 0) {
                        if (std::chrono::steady_clock::now() >= grace_deadline) {
                            kill(pid, SIGKILL);
                            waitpid(pid, &status, 0);
                            break;
                        }
                        usleep(50000);  // 50ms 轮询间隔
                    }
                    child_reaped = true;
                    break;
                }

                // poll() 超时上限 100ms 平衡响应性与 CPU 开销
                struct pollfd pfd;
                pfd.fd = pipefd[0];
                pfd.events = POLLIN;
                pfd.revents = 0;
                auto remaining_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    deadline - now).count();
                int poll_timeout = static_cast<int>(std::min<long>(remaining_ms, 100));
                int pr = poll(&pfd, 1, poll_timeout);
                if (pr > 0 && (pfd.revents & POLLIN)) {
                    ssize_t n = read(pipefd[0], buf.data(), buf.size());
                    if (n > 0) {
                        output.append(buf.data(), n);
                    } else if (n == 0) {
                        // EOF: 子进程关闭 stdout, 等待真正退出 (WNOHANG 避免阻塞)
                        while (waitpid(pid, &status, WNOHANG) != pid) {
                            usleep(10000);
                        }
                        child_reaped = true;
                        break;
                    }
                } else if (pr < 0 && errno != EINTR) break;  // poll 错误
            }

            close(pipefd[0]);

            // 兜底 reap: 异常路径退出循环时确保无僵尸进程
            if (!child_reaped) waitpid(pid, &status, 0);

            return {
                {"command", command},
                {"output", output},
                {"exit_code", WIFEXITED(status) ? WEXITSTATUS(status) : -1},
                {"timed_out", timed_out},
                {"signaled", WIFSIGNALED(status)},
                {"term_signal", WIFSIGNALED(status) ? WTERMSIG(status) : 0}
            };
        }
    );

    // shell/which
    registry.register_tool_function(
        "shell/which",
        ::agenticdsl::ToolMetadata{
            .name = "shell/which",
            .description = "Locate executable in PATH",
            .domain = "shell",
            .category = ::agenticdsl::ToolCategory::ReadOnly,
            .min_layer = ::agenticdsl::LayerProfile::Workflow,
            .approval = ::agenticdsl::ApprovalPolicy{
                .requires_approval_in_plan = false,
                .requires_approval_in_agent = false,
                .requires_approval_in_yolo = false,
                .force_approval_always = false
            },
            .allowed_layers = {::agenticdsl::LayerProfile::Workflow}
        },
        [](const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
            std::string name = str_arg(args, "name");
            if (name.empty()) throw std::runtime_error("name is required");

            // 检查 PATH
            const char* path_env = std::getenv("PATH");
            if (!path_env) return {{"name", name}, {"found", false}};

            std::string path_str(path_env);
            size_t pos = 0;
            while (pos < path_str.size()) {
                size_t next = path_str.find(':', pos);
                if (next == std::string::npos) next = path_str.size();
                std::string dir = path_str.substr(pos, next - pos);
                std::string full = dir + "/" + name;
                if (access(full.c_str(), X_OK) == 0) {
                    return {{"name", name}, {"path", full}, {"found", true}};
                }
                pos = next + 1;
            }
            return {{"name", name}, {"found", false}};
        }
    );

    // shell/env
    registry.register_tool_function(
        "shell/env",
        ::agenticdsl::ToolMetadata{
            .name = "shell/env",
            .description = "Get environment variable",
            .domain = "shell",
            .category = ::agenticdsl::ToolCategory::ReadOnly,
            .min_layer = ::agenticdsl::LayerProfile::Workflow,
            .approval = ::agenticdsl::ApprovalPolicy{
                .requires_approval_in_plan = false,
                .requires_approval_in_agent = false,
                .requires_approval_in_yolo = false,
                .force_approval_always = false
            },
            .allowed_layers = {::agenticdsl::LayerProfile::Workflow}
        },
        [](const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
            std::string name = str_arg(args, "name");
            if (name.empty()) {
                throw std::runtime_error("name is required");
            }
            const char* val = std::getenv(name.c_str());
            return {
                {"name", name},
                {"value", val ? std::string(val) : ""},
                {"set", val != nullptr}
            };
        }
    );
}