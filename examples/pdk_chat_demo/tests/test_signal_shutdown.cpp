// tests/test_signal_shutdown.cpp
// Subprocess regression tests for SIGINT/SIGTERM shutdown safety
// Fixes use-after-unload SIGSEGV when signal fires during plugin load or early shutdown

#include <catch_amalgamated.hpp>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <fstream>
#include <string>
#include <array>

namespace {

// Run pdk_chat_demo with optional env overrides, capture exit code + stderr.
// Returns true if process exited cleanly via _exit() (no SIGSEGV).
struct SubprocessResult {
  int exit_code;
  bool signaled;
  int signal;
  std::string stderr_output;
};

SubprocessResult run_pdk_chat_demo(const std::string& config_body) {
  // Write config to /tmp/chat_demo_invalid_<pid>.json
  char config_path[64];
  snprintf(config_path, sizeof(config_path),
           "/tmp/chat_demo_invalid_%d.json", getpid());
  {
    std::ofstream f(config_path);
    f << config_body;
  }

  int pipefd[2];
  REQUIRE(pipe(pipefd) == 0);

  pid_t pid = fork();
  REQUIRE(pid >= 0);

  if (pid == 0) {
    // Child: redirect stderr → pipe, exec demo
    close(pipefd[0]);
    dup2(pipefd[1], STDERR_FILENO);
    close(pipefd[1]);
    setenv("PDK_CHAT_DEMO_CONFIG", config_path, 1);
    execl("./build/examples/pdk_chat_demo/pdk_chat_demo",
          "pdk_chat_demo", "--mock", (char*)nullptr);
    _exit(127);  // exec failed
  }

  // Parent: read stderr
  close(pipefd[1]);
  std::array<char, 4096> buf;
  std::string out;
  ssize_t n;
  while ((n = read(pipefd[0], buf.data(), buf.size())) > 0) {
    out.append(buf.data(), n);
  }
  close(pipefd[0]);

  int status = 0;
  waitpid(pid, &status, 0);

  SubprocessResult r;
  r.stderr_output = out;
  if (WIFSIGNALED(status)) {
    r.signaled = true;
    r.signal = WTERMSIG(status);
    r.exit_code = -1;
  } else {
    r.signaled = false;
    r.signal = 0;
    r.exit_code = WEXITSTATUS(status);
  }
  unlink(config_path);
  return r;
}

}  // namespace

TEST_CASE("YAML validation failure exits cleanly without SIGSEGV",
          "[signal_shutdown][regression]") {
  // Deliberately malformed YAML frontmatter to trigger DSL validator failure
  const std::string bad_config = R"({
    "schema_version":"1.0",
    "app_id":"test",
    "providers":{},
    "agent":{"provider":"mock","model":"test","system_prompt":"",
             "DSL_FILE":"nonexistent-malformed-file.md"}
  })";

  auto r = run_pdk_chat_demo(bad_config);

  INFO("stderr: " << r.stderr_output);
  CHECK_FALSE(r.signaled);  // MUST NOT die from SIGSEGV
  CHECK(r.signal != SIGSEGV);
  CHECK(r.exit_code != 0);  // Expected: validation error code
}

TEST_CASE("SIGTERM during startup exits cleanly without SIGSEGV",
          "[signal_shutdown][regression]") {
  // Start a valid demo, send SIGTERM mid-load, expect clean exit
  const std::string valid_config = R"({
    "schema_version":"1.0",
    "app_id":"test",
    "providers":{},
    "agent":{"provider":"mock","model":"test","system_prompt":"You are helpful."}
  })";

  int pipefd[2];
  REQUIRE(pipe(pipefd) == 0);

  char config_path[64];
  snprintf(config_path, sizeof(config_path),
           "/tmp/chat_demo_sigterm_%d.json", getpid());
  {
    std::ofstream f(config_path);
    f << valid_config;
  }

  pid_t pid = fork();
  REQUIRE(pid >= 0);
  if (pid == 0) {
    close(pipefd[0]);
    dup2(pipefd[1], STDERR_FILENO);
    close(pipefd[1]);
    setenv("PDK_CHAT_DEMO_CONFIG", config_path, 1);
    execl("./build/examples/pdk_chat_demo/pdk_chat_demo",
          "pdk_chat_demo", "--mock", (char*)nullptr);
    _exit(127);
  }
  close(pipefd[1]);

  // Give demo time to load plugins + start loop
  usleep(500000);  // 500ms
  kill(pid, SIGTERM);

  int status = 0;
  waitpid(pid, &status, 0);

  std::array<char, 4096> buf;
  std::string out;
  ssize_t n;
  while ((n = read(pipefd[0], buf.data(), buf.size())) > 0) {
    out.append(buf.data(), n);
  }
  close(pipefd[0]);

  INFO("stderr: " << out);
  CHECK_FALSE(WIFSIGNALED(status));
  CHECK(WTERMSIG(status) != SIGSEGV);
  unlink(config_path);
}