// src/modules/skill_interpreter/skill_child_main.cpp
// 功能描述：SKILL.md 子进程入口函数。在 posix_spawn + execve 后运行，
//          负责：环境变量校验 → SKILL.md 解析（此时无 seccomp）→
//          prctl(NO_NEW_PRIVS) → seccomp BPF 加载 → 命令式 DSL 解释器主循环。
// 设计依据：ADR-0055 + openspec/changes/skill-interpreter-real-loading/design.md
//          (Decision 7/8)
// 作者：AgenticDSL SkillInterpreter change
// 最后修改日期：2026-07-22

#include "agenticdsl/skill/skill_interpreter.h"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

// Linux 特有头文件
#ifdef __linux__
#include <fcntl.h>
#include <linux/audit.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>

// inja 模板引擎
#include <inja/inja.hpp>

namespace agenticdsl {

// ============================================================
// 退出码约定
// ============================================================
constexpr int EXIT_CHILD_PARSE_ERROR = 64;
constexpr int EXIT_CHILD_ENV_MISSING = 70;
constexpr int EXIT_CHILD_PRCTL_ERROR = 71;
constexpr int EXIT_CHILD_SECCOMP_ERROR = 72;
constexpr int EXIT_CHILD_THREAD_LEAK = 73;

// ============================================================
// ParsedSkill — SKILL.md 解析结果
// ============================================================
struct SkillStatement {
  enum class Type {
    CallTool,
    Assign,
    Return,
    EmitEvent,
    LlmGenerate,
    ConsumeBudget,
  };

  Type type;
  std::string target;          // assign 的左值 / call_tool 的工具名
  nlohmann::json args_json;    // call_tool / emit_event / consume_budget 的参数（JSON）
  std::string args_yaml;       // 原始参数文本（用于 inja 插值）
};

struct ParsedSkill {
  std::string name;
  std::string version;
  std::string description;
  std::vector<SkillStatement> statements;
};

// ============================================================
// 工具函数：行导向解析 + IPC
// ============================================================

/// 从 stdin（pipe_in）读取一行
static std::string read_ipc_line(int fd) {
  std::string buf;
  char ch;
  while (true) {
    ssize_t n = read(fd, &ch, 1);
    if (n <= 0) break;
    if (ch == '\n') break;
    buf += ch;
  }
  return buf;
}

/// 写 IPC 响应到 stdout（pipe_out）
static void write_ipc_line(int fd, const std::string& msg) {
  std::string framed = msg + '\n';
  const char* data = framed.data();
  size_t remaining = framed.size();
  while (remaining > 0) {
    ssize_t n = write(fd, data, remaining);
    if (n < 0) {
      if (errno == EINTR) continue;
      return;
    }
    data += n;
    remaining -= static_cast<size_t>(n);
  }
}

/// IPC 请求辅助函数
static nlohmann::json send_ipc_request(int ipc_out, int ipc_in,
                                        const std::string& method,
                                        const nlohmann::json& params) {
  nlohmann::json req;
  req["method"] = method;
  req["params"] = params;
  write_ipc_line(ipc_out, req.dump());

  // 阻塞等待响应
  std::string resp_line = read_ipc_line(ipc_in);
  if (resp_line.empty()) {
    throw std::runtime_error("IPC: empty response (parent died?)");
  }

  auto resp = nlohmann::json::parse(resp_line);
  if (!resp.value("ok", false)) {
    throw std::runtime_error("IPC: " + resp.value("error", "unknown error"));
  }
  return resp.value("result", nlohmann::json::object());
}

// ============================================================
// inja 变量插值
// ============================================================

/// 使用 inja 模板引擎渲染 `{{var}}` 插值
/// V1 仅支持 inja 默认语法（{{var}}），不支持 ${var}
static std::string render_template(const std::string& text,
                                    const inja::json& data) {
  inja::Environment env;
  // V1 不扩展 LexerConfig — 仅支持 inja 默认 {{var}}
  return env.render(text, data);
}

// ============================================================
// parse_skill_file — 行导向递归下降解析器
// ============================================================

static ParsedSkill parse_skill_file(const std::string& path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    throw std::runtime_error("Cannot open file: " + path);
  }

  // 读取全部内容
  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string content = buffer.str();

  // 解析 frontmatter (YAML between --- ... ---)
  ParsedSkill skill;

  auto front_start = content.find("---");
  if (front_start == std::string::npos) {
    throw std::runtime_error("Missing frontmatter delimiter (---)");
  }

  auto front_end = content.find("---", front_start + 3);
  if (front_end == std::string::npos) {
    throw std::runtime_error("Missing closing frontmatter delimiter (---)");
  }

  std::string yaml_str = content.substr(front_start + 3, front_end - front_start - 3);
  auto fm = YAML::Load(yaml_str);

  skill.name = fm["name"].as<std::string>("");
  skill.version = fm["version"].as<std::string>("");
  skill.description = fm["description"].as<std::string>("");

  if (skill.name.empty()) {
    throw std::runtime_error("missing required field: name");
  }

  // 解析 body（frontmatter 之后的内容）
  std::string body = content.substr(front_end + 3);
  std::istringstream body_stream(body);
  std::string line;

  while (std::getline(body_stream, line)) {
    // 去除首尾空白
    auto trim_start = line.find_first_not_of(" \t\r");
    if (trim_start == std::string::npos) continue;
    line = line.substr(trim_start);

    // 跳过空行和注释
    if (line.empty() || line[0] == '#') continue;

    // 解析语句
    SkillStatement stmt;

    if (line.rfind("call_tool(", 0) == 0 || line.rfind("call_tool (", 0) == 0) {
      stmt.type = SkillStatement::Type::CallTool;
      // call_tool("name", {args})
      auto paren_open = line.find('(');
      auto paren_close = line.rfind(')');
      if (paren_open == std::string::npos || paren_close == std::string::npos) {
        throw std::runtime_error("call_tool: missing parentheses");
      }
      std::string inner = line.substr(paren_open + 1, paren_close - paren_open - 1);

      // 提取工具名（第一个引号字符串）
      auto q1 = inner.find('"');
      if (q1 == std::string::npos) throw std::runtime_error("call_tool: missing tool name");
      auto q2 = inner.find('"', q1 + 1);
      if (q2 == std::string::npos) throw std::runtime_error("call_tool: unclosed tool name");
      stmt.target = inner.substr(q1 + 1, q2 - q1 - 1);

      // 剩余部分为 args JSON
      auto args_start = inner.find('{', q2);
      auto args_end = inner.rfind('}');
      if (args_start != std::string::npos && args_end != std::string::npos) {
        stmt.args_yaml = inner.substr(args_start, args_end - args_start + 1);
      } else {
        stmt.args_yaml = "{}";
      }

    } else if (line.rfind("assign ", 0) == 0) {
      stmt.type = SkillStatement::Type::Assign;
      // assign key = expr
      auto eq_pos = line.find('=');
      if (eq_pos == std::string::npos) {
        throw std::runtime_error("assign: missing '='");
      }
      stmt.target = line.substr(7, eq_pos - 7);
      // 去除 target 首尾空白
      auto t_start = stmt.target.find_first_not_of(" \t");
      auto t_end = stmt.target.find_last_not_of(" \t");
      if (t_start != std::string::npos && t_end != std::string::npos) {
        stmt.target = stmt.target.substr(t_start, t_end - t_start + 1);
      }
      stmt.args_yaml = line.substr(eq_pos + 1);
      // 去除 args_yaml 首尾空白
      auto a_start = stmt.args_yaml.find_first_not_of(" \t");
      if (a_start != std::string::npos) {
        stmt.args_yaml = stmt.args_yaml.substr(a_start);
      }

    } else if (line.rfind("return ", 0) == 0 || line == "return") {
      stmt.type = SkillStatement::Type::Return;
      if (line.size() > 7) {
        stmt.args_yaml = line.substr(7);
        auto a_start = stmt.args_yaml.find_first_not_of(" \t");
        if (a_start != std::string::npos) {
          stmt.args_yaml = stmt.args_yaml.substr(a_start);
        }
      } else {
        stmt.args_yaml = "{}";
      }

    } else if (line.rfind("emit_event(", 0) == 0) {
      stmt.type = SkillStatement::Type::EmitEvent;
      auto paren_open = line.find('(');
      auto paren_close = line.rfind(')');
      if (paren_open == std::string::npos || paren_close == std::string::npos) {
        throw std::runtime_error("emit_event: missing parentheses");
      }
      std::string inner = line.substr(paren_open + 1, paren_close - paren_open - 1);
      // topic, {payload}
      auto comma = inner.find(',');
      if (comma == std::string::npos) throw std::runtime_error("emit_event: missing comma");
      auto q1 = inner.find('"');
      auto q2 = inner.find('"', q1 + 1);
      if (q1 == std::string::npos || q2 == std::string::npos)
        throw std::runtime_error("emit_event: missing topic string");
      stmt.target = inner.substr(q1 + 1, q2 - q1 - 1);
      stmt.args_yaml = inner.substr(comma + 1);
      auto a_start = stmt.args_yaml.find_first_not_of(" \t");
      if (a_start != std::string::npos) {
        stmt.args_yaml = stmt.args_yaml.substr(a_start);
      }

    } else if (line.rfind("llm_generate(", 0) == 0) {
      stmt.type = SkillStatement::Type::LlmGenerate;
      auto paren_open = line.find('(');
      auto paren_close = line.rfind(')');
      if (paren_open == std::string::npos || paren_close == std::string::npos) {
        throw std::runtime_error("llm_generate: missing parentheses");
      }
      stmt.args_yaml = line.substr(paren_open + 1, paren_close - paren_open - 1);

    } else if (line.rfind("consume_budget(", 0) == 0) {
      stmt.type = SkillStatement::Type::ConsumeBudget;
      auto paren_open = line.find('(');
      auto paren_close = line.rfind(')');
      if (paren_open == std::string::npos || paren_close == std::string::npos) {
        throw std::runtime_error("consume_budget: missing parentheses");
      }
      stmt.args_yaml = line.substr(paren_open + 1, paren_close - paren_open - 1);

    } else {
      throw std::runtime_error("Unknown statement: " + line);
    }

    skill.statements.push_back(std::move(stmt));
  }

  return skill;
}

// ============================================================
// seccomp BPF 过滤器（Decision 3）
// ============================================================

#ifdef __linux__
#define ALLOW BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW)
#define KILL  BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS)
#define JEQ(syscall) BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, syscall, 0, 1)

// 辅助宏：检查 syscall 并 ALLOW，否则跳到下一条
#define CHECK_SYSCALL(syscall) \
  JEQ(syscall),               \
  ALLOW

static int apply_seccomp_filter() {
  // V1 BPF 白名单（spike 后定稿，25-30 syscall）
  // 架构检查 + 白名单 + default KILL
  struct sock_filter filter[] = {
    // [0] Architecture check: must be x86_64
    BPF_STMT(BPF_LD | BPF_W | BPF_ABS,
             offsetof(struct seccomp_data, arch)),
    BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, AUDIT_ARCH_X86_64, 1, 0),
    KILL,

    // [2] Load syscall number
    BPF_STMT(BPF_LD | BPF_W | BPF_ABS,
             offsetof(struct seccomp_data, nr)),

    // === V1 白名单（25 syscalls, spike 后定稿）===
    // I/O
    CHECK_SYSCALL(__NR_read),
    CHECK_SYSCALL(__NR_write),
    CHECK_SYSCALL(__NR_close),
    // 进程控制
    CHECK_SYSCALL(__NR_exit),
    CHECK_SYSCALL(__NR_exit_group),
    CHECK_SYSCALL(__NR_getpid),
    CHECK_SYSCALL(__NR_getrandom),
    // 内存管理
    CHECK_SYSCALL(__NR_brk),
    CHECK_SYSCALL(__NR_mmap),
    CHECK_SYSCALL(__NR_munmap),
    CHECK_SYSCALL(__NR_mprotect),
    CHECK_SYSCALL(__NR_madvise),
    // 信号处理（C++ 异常展开必需）
    CHECK_SYSCALL(__NR_rt_sigaction),
    CHECK_SYSCALL(__NR_rt_sigprocmask),
    CHECK_SYSCALL(__NR_sigaltstack),
    // 线程/同步
    CHECK_SYSCALL(__NR_futex),
    // 时间
    CHECK_SYSCALL(__NR_clock_gettime),
    CHECK_SYSCALL(__NR_gettimeofday),
#if defined(__NR_clock_nanosleep)
    CHECK_SYSCALL(__NR_clock_nanosleep),
#endif
#if defined(__NR_nanosleep)
    CHECK_SYSCALL(__NR_nanosleep),
#endif
    // 文件（read-only: 用于 nlohmann::json 内部 stat 等）
#if defined(__NR_newfstatat)
    CHECK_SYSCALL(__NR_newfstatat),
#elif defined(__NR_fstatat64)
    CHECK_SYSCALL(__NR_fstatat64),
#endif
#if defined(__NR_readlink)
    CHECK_SYSCALL(__NR_readlink),
#endif
    // 其他
    CHECK_SYSCALL(__NR_writev),
    CHECK_SYSCALL(__NR_pread64),
#if defined(__NR_lseek)
    CHECK_SYSCALL(__NR_lseek),
#endif
    // set_robust_list / get_robust_list (glibc 内部使用)
#if defined(__NR_set_robust_list)
    CHECK_SYSCALL(__NR_set_robust_list),
#endif
#if defined(__NR_get_robust_list)
    CHECK_SYSCALL(__NR_get_robust_list),
#endif
    // rseq (restartable sequences, glibc 2.35+)
#if defined(__NR_rseq)
    CHECK_SYSCALL(__NR_rseq),
#endif

    // 默认：KILL
    KILL,
  };

  struct sock_fprog prog;
  prog.len = sizeof(filter) / sizeof(filter[0]);
  prog.filter = filter;

  // prctl 必须在 seccomp 之前调用
  int ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
  if (ret != 0) {
    return ret;  // 返回 -1/EPERM
  }

  // 加载 seccomp BPF（使用 syscall 直接调用，glibc 无 wrapper）
  ret = syscall(SYS_seccomp, SECCOMP_SET_MODE_FILTER, 0, &prog);
  return ret;
}
#endif  // __linux__

// ============================================================
// interpret_main_loop — 命令式 DSL 解释器主循环
// ============================================================

static int interpret_main_loop(const ParsedSkill& skill,
                                int ipc_in, int ipc_out, int ipc_err) {
  // 内部变量表（用于 assign / {{var}} 插值）
  inja::json vars;
  vars["args"] = inja::json::object();

  auto eval_args = [&](const std::string& raw) -> nlohmann::json {
    if (raw.empty()) return nlohmann::json::object();
    // 先进行 inja 插值
    std::string rendered = render_template(raw, vars);
    // 尝试解析为 JSON
    try {
      return nlohmann::json::parse(rendered);
    } catch (...) {
      // 不是 JSON，作为字符串处理
      return rendered;
    }
  };

  auto eval_expr = [&](const std::string& raw) -> nlohmann::json {
    std::string trimmed = raw;
    auto s = trimmed.find_first_not_of(" \t");
    if (s != std::string::npos) trimmed = trimmed.substr(s);
    auto e = trimmed.find_last_not_of(" \t\r\n");
    if (e != std::string::npos) trimmed = trimmed.substr(0, e + 1);

    // 先检查是否是变量引用
    if (vars.contains(trimmed)) {
      return vars[trimmed];
    }

    // 尝试 JSON 解析
    std::string rendered = render_template(trimmed, vars);
    try {
      return nlohmann::json::parse(rendered);
    } catch (...) {
      return rendered;
    }
  };

  size_t max_steps_friendly = 50;  // 友好提示（非安全边界）
  size_t step = 0;

  for (const auto& stmt : skill.statements) {
    if (++step > max_steps_friendly) {
      // 仅作友好提示
      dprintf(ipc_err, "[skill] step %zu exceeds friendly max_steps %zu\n",
              step, max_steps_friendly);
    }

    switch (stmt.type) {
      case SkillStatement::Type::CallTool: {
        nlohmann::json args = eval_args(stmt.args_yaml);
        nlohmann::json params;
        params["name"] = stmt.target;
        params["args"] = args;

        try {
          nlohmann::json result = send_ipc_request(ipc_out, ipc_in,
                                                    "call_tool", params);
          // 将结果存入变量表（工具名去掉 / 作为 key）
          std::string var_name = stmt.target;
          std::replace(var_name.begin(), var_name.end(), '/', '_');
          vars[var_name] = result;
        } catch (const std::exception& e) {
          dprintf(ipc_err, "[skill] call_tool(%s) failed: %s\n",
                  stmt.target.c_str(), e.what());
          vars[stmt.target + "_error"] = e.what();
        }
        break;
      }

      case SkillStatement::Type::Assign: {
        // assign key = expr
        vars[stmt.target] = eval_expr(stmt.args_yaml);
        break;
      }

      case SkillStatement::Type::Return: {
        nlohmann::json value = eval_expr(stmt.args_yaml);
        nlohmann::json params;
        params["value"] = value;
        write_ipc_line(ipc_out,
            (nlohmann::json{{"method", "return"}, {"params", params}}).dump());
        // 等待父进程确认（可选）
        read_ipc_line(ipc_in);
        _exit(0);
      }

      case SkillStatement::Type::EmitEvent: {
        nlohmann::json payload = eval_args(stmt.args_yaml);
        nlohmann::json params;
        params["topic"] = stmt.target;
        params["payload"] = payload;

        try {
          send_ipc_request(ipc_out, ipc_in, "emit_event", params);
        } catch (const std::exception& e) {
          dprintf(ipc_err, "[skill] emit_event(%s) failed: %s\n",
                  stmt.target.c_str(), e.what());
        }
        break;
      }

      case SkillStatement::Type::LlmGenerate: {
        nlohmann::json params;
        params["prompt"] = stmt.args_yaml;

        try {
          nlohmann::json result = send_ipc_request(ipc_out, ipc_in,
                                                    "llm_generate", params);
          vars["llm_result"] = result;
        } catch (const std::exception& e) {
          dprintf(ipc_err, "[skill] llm_generate failed: %s\n", e.what());
          vars["llm_error"] = e.what();
        }
        break;
      }

      case SkillStatement::Type::ConsumeBudget: {
        nlohmann::json parsed = eval_args(stmt.args_yaml);
        double amount = 0.01;
        if (parsed.is_number()) {
          amount = parsed.get<double>();
        }
        nlohmann::json params;
        params["amount"] = amount;

        try {
          send_ipc_request(ipc_out, ipc_in, "consume_budget", params);
        } catch (const std::exception& e) {
          dprintf(ipc_err, "[skill] consume_budget failed: %s\n", e.what());
          _exit(0);  // parent killed us, exit silently
        }
        break;
      }
    }
  }

  // 执行完毕无 return → 默认返回 null
  write_ipc_line(ipc_out,
      (nlohmann::json{{"method", "return"}, {"params", {{"value", nullptr}}}}).dump());
  read_ipc_line(ipc_in);  // 等待确认（可忽略）
  _exit(0);
}

// ============================================================
// skill_child_main — 子进程入口
// ============================================================

int skill_child_main(int argc, char** argv) {
#ifdef __linux__
  (void)argc;
  (void)argv;

  // === Step 0: 设置 PDEATHSIG（父进程死亡时子进程自动 SIGKILL）===
  // H8: PR_SET_PDEATHSIG 在父进程先于子进程 exit 时有效
  prctl(PR_SET_PDEATHSIG, SIGKILL);

  // === Step 1: 环境变量校验 ===
  const char* skill_path = getenv("SKILL_PATH");
  const char* ipc_in_str = getenv("SKILL_IPC_IN");
  const char* ipc_out_str = getenv("SKILL_IPC_OUT");
  const char* ipc_err_str = getenv("SKILL_IPC_ERR");

  if (!skill_path || !ipc_in_str || !ipc_out_str || !ipc_err_str) {
    std::fprintf(stderr, "skill_child_main: missing required env vars\n");
    if (skill_path) std::fprintf(stderr, "  SKILL_PATH=%s\n", skill_path);
    else            std::fprintf(stderr, "  SKILL_PATH=(unset)\n");
    if (ipc_in_str) std::fprintf(stderr, "  SKILL_IPC_IN=%s\n", ipc_in_str);
    else            std::fprintf(stderr, "  SKILL_IPC_IN=(unset)\n");
    if (ipc_out_str) std::fprintf(stderr, "  SKILL_IPC_OUT=%s\n", ipc_out_str);
    else             std::fprintf(stderr, "  SKILL_IPC_OUT=(unset)\n");
    if (ipc_err_str) std::fprintf(stderr, "  SKILL_IPC_ERR=%s\n", ipc_err_str);
    else             std::fprintf(stderr, "  SKILL_IPC_ERR=(unset)\n");
    _exit(EXIT_CHILD_ENV_MISSING);
  }

  int ipc_in = std::atoi(ipc_in_str);
  int ipc_out = std::atoi(ipc_out_str);
  int ipc_err = std::atoi(ipc_err_str);

  // === Step 1b: C4 invariant: Threads must be 1 at entry ===
  {
    std::ifstream f("/proc/self/status");
    std::string line;
    int threads = 1;
    while (std::getline(f, line)) {
      if (line.rfind("Threads:", 0) == 0) {
        threads = std::stoi(line.substr(8));
        break;
      }
    }
    if (threads != 1) {
      std::fprintf(stderr,
          "skill_child_main: Threads=%d at entry (expected 1). ABORT.\n",
          threads);
      _exit(EXIT_CHILD_THREAD_LEAK);
    }
  }

  // === Step 2: 解析 SKILL.md（此时无 seccomp，可 malloc/yaml 自由使用）===
  ParsedSkill skill;
  try {
    skill = parse_skill_file(skill_path);
  } catch (const std::exception& e) {
    std::fprintf(stderr, "skill_child_main: parse failed: %s\n", e.what());
    _exit(EXIT_CHILD_PARSE_ERROR);
  }

  // === Step 3: 资源限制 ===
  // 设置子进程资源限制
  struct rlimit fd_lim;
  fd_lim.rlim_cur = 8;    // 限制 fd 数量
  fd_lim.rlim_max = 8;
  setrlimit(RLIMIT_NOFILE, &fd_lim);

  struct rlimit as_lim;
  as_lim.rlim_cur = 256 * 1024 * 1024;  // 256MB 地址空间上限
  as_lim.rlim_max = 256 * 1024 * 1024;
  setrlimit(RLIMIT_AS, &as_lim);

  // === Step 4: prctl(NO_NEW_PRIVS) ===
  if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
    std::fprintf(stderr, "skill_child_main: prctl failed: %s\n",
                 std::strerror(errno));
    _exit(EXIT_CHILD_PRCTL_ERROR);
  }

  // === Step 5: 加载 seccomp BPF ===
  if (apply_seccomp_filter() != 0) {
    std::fprintf(stderr, "skill_child_main: seccomp load failed: %s\n",
                 std::strerror(errno));
    _exit(EXIT_CHILD_SECCOMP_ERROR);
  }

  // === Step 6: 进入解释器主循环 ===
  return interpret_main_loop(skill, ipc_in, ipc_out, ipc_err);

#else
  (void)argc;
  (void)argv;
  std::fprintf(stderr, "skill_child_main: requires Linux\n");
  _exit(EXIT_CHILD_ENV_MISSING);
#endif
}

}  // namespace agenticdsl