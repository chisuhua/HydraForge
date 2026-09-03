# U4: AgentForge 第二领域 Agent 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 AgentForge (`/workspace/project/AgentForge`) 中建立第一个可测试的 `coding_assistant` Agent 基线，再新增第二个领域 Agent `doc_writer`，验证 HydraForge PDK 的跨领域复用性。U4 是 Phase 7a 启动条件 C1（AgentForge ≥2 agents）的唯一代码解锁项。

**Architecture:** 保持 dual-repo 架构（HydraForge + AgentForge 独立 repo），U4 通过 FetchContent + pinned commit 消费 HydraForge Runtime/PDK。两个 Agent 共享同一套 HydraForgeClient + AgentRegistry（如果前置阶段已接入），但 handler 与测试用例独立。Agent Loop 统一使用 React，不引入 PlanExecute/ForkJoin/MCP/真实云端 LLM。

**Tech Stack:** C++20 / Catch2 / HydraForge PDK (`DEFINE_AGENT` + `DECLARE_TOOL`) / HydraForge Runtime (FetchContent) / MockLLMProvider

**关联计划**:
- [docs/superpowers/plans/2026-07-15-phase6-agentforge-mvp.md](./2026-07-15-phase6-agentforge-mvp.md) — Sprint 24 总体计划
- [docs/proposals/implementation/agentforge-mvp-blueprint.md](../proposals/implementation/agentforge-mvp-blueprint.md) — AgentForge 蓝图设计
- HydraForge roadmap.md §Phase 7 — U4 作为 Phase 7a C1 解锁项

**关联 ADR**:
- ADR-0019 IInteractionBus + TUI Chat MVP
- ADR-0020 Thread Model Isolation
- ADR-0021 PDK Design (含 §7 Dual-Repo Policy)
- ADR-0022 Plugin Loading
- ADR-0031 Execution Policy
- ADR-0043 PDK Tool Naming Convention
- ADR-0050 Phase 6 Strategic Evaluation (Solo Dev 重评)
- ADR-0051 Phase 6 PDK Composition Spike (含 §后 #9 AgentForge 同人项目约束)

---

## 0. Repo 关系决策：保持 dual-repo，不引入 submodule

### 0.1 是否将 AgentForge 作为 HydraForge 的 submodule？

**结论：不推荐。**

HydraForge 当前 `.gitmodules` 已有 3 个 submodule：

```text
external/nlohmann_json    (vendor 第三方依赖)
external/inja             (vendor 第三方依赖)
external/yaml-cpp         (vendor 第三方依赖)
```

这些是 vendor 性质（只读、跟随上游、不反向贡献）。AgentForge 与它们性质不同。

### 0.2 对比分析

| 维度 | 现状 (dual-repo + FetchContent) | AgentForge 作为 submodule | HydraForge 子目录 `examples/agentforge/` |
|---|---|---|---|
| **ADR-0021 §7 Dual-Repo Policy** | ✅ 符合（AgentForge 是下游消费者角色） | ❌ 违反（submodule 是 vendor 关系，不是消费者） | ❌ 违反（"monorepo 内"反方向） |
| **蓝图 §二 Repo 决策** (2026-07-15) | ✅ 符合（明确新建 `chisuhua/AgentForge`） | ❌ 违反（蓝图决策日已排除） | ❌ 违反 |
| **PDK 复用性验证场景** | ✅ 跨 repo 物理隔离 = 真实 PDK 消费者场景 | ⚠️ submodule 内引用 = 同一 repo，验证力度下降 | ⚠️ 同 repo，验证力度下降 |
| **构建影响** | HydraForge 构建独立，AgentForge 构建独立 | AgentForge 改动可能触发 HydraForge 重新拉 submodule | HydraForge 构建时一并构建 AgentForge |
| **独立 ship 节奏** | ✅ AgentForge 可独立版本化 | ⚠️ HydraForge 锁 commit 影响 AgentForge | ❌ HydraForge Sprint 节奏同步 |
| **团队责任划分** | ✅ AgentForge 维护者独立 | ⚠️ HydraForge 维护者需管理 submodule | ❌ HydraForge 维护者需同时维护下游 |
| **`scripts/sync-pdk.sh` 已 ship** | ✅ 已有自动化同步工具 (commit `d7612cc`) | N/A | N/A |
| **U4 实施成本** | 0 额外开销（现状） | +1d（submodule 初始化 + README 更新 + CI 改造） | +1d（move 代码 + git 历史保留） |

### 0.3 Oracle 复审（2026-09-03）—— 用数据证伪"同步开发"前提

用户初次提案时建议"用 submodule 一起开发更方便（因为 HydraForge 和 AgentForge 同步开始）"。Oracle session `ses_f988ee9adffe86hvD0UBTb3I6h` 用 commit 频率数据证伪该前提：

| 仓库 | 近 3 周 commits | 每周分布 | 最后提交 |
|---|---|---|---|
| HydraForge | **292** | 160 / 92 / 40（≈6-23 commits/天） | 2026-09-03（今天） |
| AgentForge | **0** | 0 / 0 / 0 | **2026-07-16（49 天前，共仅 5 个 commit）** |

**Oracle 核心论断**：

1. **方向性错误**：依赖方向是 AgentForge→HydraForge；把 AgentForge 嵌进 HydraForge 是**反向嵌入**（engine 仓库包含自己的下游消费者），违反分层
2. **FetchContent + pinned commit ≈ submodule 语义**（显式 pin、显式 bump、可回滚）但无 git 工作流税
3. **B/C 迁移成本 4-8h > U4 任务本身**（12-20h 中仅第二 Agent 是核心）
4. **C1 验证目标受损**：PDK 外部消费者复用性的物理隔离证据链会失效
5. **用户真正痛点是 Day 1 FetchContent 摩擦**（根因 `GIT_TAG main` 漂移），不是 dual-repo 架构本身——本计划 §0.4 已用 pinned commit 修复

**Oracle 推荐**：方案 A（dual-repo）继续执行；Revisit 触发条件 = AgentForge 连续 2 周 ≥20 commits/周 且 pin bump 每周 >3 次。

### 0.4 已排除的备选方案

蓝图 §二 Repo 决策已系统化排除 4 个备选，本次不再重复论证：

1. ❌ 复用 `chisuhua/hydraforge-pdk`（PDK 是反向生命周期）
2. ❌ HydraForge monorepo 内 `examples/agent_chat/`（monorepo 是 engine 职责）
3. ❌ 等 HydraForge Runtime install rules（多 1-2 周）
4. ❌ AgentForge 作为 HydraForge submodule（本节 + Oracle 复审明确排除）

### 0.5 dual-repo 同步策略（U4 必须遵循）

U4 实施期间执行以下步骤保证同步稳定性：

1. **pin commit，不使用 `GIT_TAG main`**（Day 1.1 已尝试 main 漂移）
2. **实施前** 同步：`cd /workspace/project/AgentForge && bash /workspace/project/HydraForge/scripts/sync-pdk.sh`
3. **实施后** 验证：`ctest --test-dir build/tests --output-on-failure`（HydraForge 端基线不变）
4. **如 HydraForge 引入破坏性改动**，回滚 AgentForge FetchContent 至上一稳定 commit，不在 U4 内修复 HydraForge 端
5. **HydraForge 端只承诺「pinned SHA 可构建」**，不承诺 main 随时可消费——main 漂移是 HydraForge 内部自由
6. **每次 U4 工作开始前** 跑 `sync-pdk.sh` 对齐 PDK 头文件

### 0.6 决策记录

| 日期 | 决策 | 依据 |
|---|---|---|
| 2026-09-03 | U4 维持 dual-repo 架构（不引入 submodule / 不下沉 monorepo） | ADR-0021 §7 + 蓝图 §二 + scripts/sync-pdk.sh 已 ship |
| 2026-09-03 | Oracle 复审确认 dual-repo 推荐 | commit 频率数据（HydraForge 292 vs AgentForge 0/3 周）证伪"同步开发"前提；B/C 方向性错误且验证目标受损 |

---

## 1. U4 目标与边界

### 1.1 目标

完成 AgentForge 第二领域 Agent `doc_writer`，使其与已建立的第一个 Agent（前置阶段建立 `coding_assistant`）并存，验证 HydraForge PDK 的跨领域复用性。U4 是 Phase 7a 启动条件 C1（AgentForge ≥2 agents）的唯一代码解锁项。

### 1.2 In-scope

- AgentForge 第一个 `coding_assistant` 最小可测试基线（前置阶段）
- AgentForge 测试框架（CMake + Catch2）
- AgentForge 最小 HydraForge Runtime/PDK 接入
- 新增 `doc_writer` Agent（U4 核心）
- 两个 Agent 的注册/路由（共享 Client 或 AgentRegistry）
- MockLLM 驱动的单元测试与集成测试
- 双 Agent 独立运行 + 错误隔离验证
- CMake 构建与 CTest 验证
- AgentForge 文档状态同步（README + ADR-AF-001）

### 1.3 Out-of-scope（U4 明确不做）

- 完整 FTXUI TUI（保持现状）
- 7 个文件工具（fs/read, fs/write, fs/edit, fs/glob, fs/grep, shell/exec, +1 ext）
- 真实 OpenAI E2E
- MCP Server（ADR-0076，Phase 7 启动后评估）
- PluginLoader 用户插件加载
- Agent-to-Agent delegation（ADR-0060 决策 2，Phase 2）
- PlanExecute / ForkJoin loop（U4 仅 React）
- 多进程 Agent
- SQLite Session 持久化
- LSP 工具适配
- 云服务部署
- HydraForge 主仓库代码修改
- AgentForge → HydraForge submodule 化（见 §0）

### 1.4 阶段规划

| 阶段 | 内容 | 估时 |
|---|---|---:|
| 阶段一 | 基线冻结 + 环境确认（dual-repo 同步） | 0.5h |
| 阶段二 | 第一 Agent `coding_assistant` 最小基线（前置） | 6-10h |
| 阶段三 | 第二 Agent `doc_writer` 契约 + 失败测试 | 1h |
| 阶段四 | 第二 Agent `doc_writer` 实现 + 路由接入 | 3-5h |
| 阶段五 | 双 Agent 集成测试 | 1-2h |
| 阶段六 | 最终验证（build/ctest/diagnostics/docs） | 1-2h |
| **合计** | **前置 + U4** | **12.5-20.5h** |

---

## 2. 当前真实基线（实施前必读）

### 2.1 AgentForge 本地仓库

路径：`/workspace/project/AgentForge`

```text
5ee1c6d docs(adr): Day 4 — Oracle Option D 路径完成, 撤销 Known Limitations #1
dc33384 docs(adr): Day 3 lessons learned — HydraForge FetchContent blocked, pivot to MockLLMProvider
82fb139 feat(sprint-24-day-2): vendor FTXUI v6.1.9 + HydraForgeClient skeleton
c123858 fix(build): Day 1.1 — resolve Threads + gate HydraForge FetchContent behind option
3f189e3 feat: bootstrap AgentForge TUI programming assistant (Sprint 24 Day 1)
```

当前工作区 untracked 文件（U4 不应自动纳入）：

```text
.gitignore (modified)
.mcp.json
.omo/
.opencode.json
AGENTS.md
```

项目自身文件清单：

```text
CMakeLists.txt
README.md
docs/ADR-AF-001-design.md
include/agentforge/hydraforge_client.h
src/hydraforge_client.cpp
src/main.cpp
```

**不存在**：`tests/`, `src/agents/`, `src/tools/`, `src/tui/`, `examples/`

### 2.2 HydraForgeClient 现状（stub）

`include/agentforge/hydraforge_client.h` 接口已声明：

```cpp
bool initialize();
bool start_session(const std::string& user_input);
void stop_session();
bool session_active() const;
static HydraForgeClient& instance();
```

`src/hydraforge_client.cpp` 全部 stub（返回 `false` 或空操作）：

```cpp
bool HydraForgeClient::initialize() { return false; }
bool HydraForgeClient::start_session(const std::string&) { return false; }
```

### 2.3 关键结论

- ❌ 没有 `tests/` 目录 → 无 AgentForge 测试基线
- ❌ 没有 `src/agents/coding_assistant.cpp` → 第一 Agent 不存在
- ❌ 没有 `src/tools/` → 无工具实现
- ⚠️ README/ADR 中的 `CodingAssistant` 是设计目标，不是已交付
- ⚠️ HydraForge 内的 `pdk/g1_coding_assistant/` 是 PDK 参考插件，不是 AgentForge 第一 Agent

**因此 U4 不能直接添加"第二个 Agent"。必须先建立第一 Agent 基线。**

---

## 3. dual-repo 同步（U4 实施前强制执行）

### Task 1.1：PDK 同步

仓库：`/workspace/project/AgentForge`

```bash
cd /workspace/project/HydraForge
git status --short
git log --oneline -5
bash /workspace/project/HydraForge/scripts/sync-pdk.sh 2>&1 | tail -20
```

**Acceptance**:
- HydraForge 工作区清洁或仅追踪已计划改动
- PDK 同步无错误（脚本 exit 0）

### Task 1.2：HydraForge 基线验证

```bash
cd /workspace/project/HydraForge
cmake --preset tests -B build/tests
cmake --build build/tests -j"$(nproc)"
ctest --test-dir build/tests --output-on-failure | tail -20
```

**Acceptance**:
- 测试基线 ≥ 184/185（不引入新失败）
- 不修改 HydraForge 源码

### Task 1.3：AgentForge skeleton build 验证

```bash
cd /workspace/project/AgentForge
git status --short
git log --oneline -5
find src include tests -maxdepth 3 -type f 2>/dev/null | sort
cmake -B build/skeleton -S .
cmake --build build/skeleton -j"$(nproc)"
./build/skeleton/agentforge
```

**预期输出**:

```text
AgentForge MVP — Sprint 24 Day 2
Status: HydraForgeClient interface ready; Day 3+ wiring
```

**Acceptance**:
- skeleton build 成功
- 当前 Day 2 输出保持
- 记录 `.gitignore` 修改与 untracked 文件（不纳入 U4 diff）

---

## 4. 阶段二：第一 Agent `coding_assistant` 最小基线

这是 U4 的硬前置。

### Task 2.1：建立测试目标与测试框架

**Files**:
- Create: `/workspace/project/AgentForge/tests/test_coding_assistant.cpp`
- Modify: `/workspace/project/AgentForge/CMakeLists.txt`

**Step 1: CMakeLists.txt 添加测试目标**

```cmake
# ---------------------------------------------------------------------------
# Tests (Sprint 25 U4 baseline)
# ---------------------------------------------------------------------------
option(AGENTFORGE_BUILD_TESTS "Build AgentForge tests" OFF)

if(AGENTFORGE_BUILD_TESTS)
    enable_testing()

    # Catch2 v3 (vendored at vendor/catch2/ or find_package)
    find_package(Catch2 3 REQUIRED)

    add_executable(agentforge_tests
        tests/test_coding_assistant.cpp
        tests/test_doc_writer.cpp
        tests/test_two_agents.cpp
    )

    target_link_libraries(agentforge_tests
        PRIVATE
            agentforge
            Catch2::Catch2WithMain
    )

    add_test(NAME agentforge_tests COMMAND agentforge_tests)
endif()
```

**Step 2: tests/test_coding_assistant.cpp 写失败测试**

```cpp
#include <catch2/catch_test_macros.hpp>
#include "agentforge/hydraforge_client.h"

TEST_CASE("coding assistant can be initialized", "[agent][baseline]") {
    agentforge::HydraForgeClient client;
    REQUIRE(client.initialize());
}

TEST_CASE("coding assistant handles a request", "[agent][baseline]") {
    agentforge::HydraForgeClient client;
    REQUIRE(client.initialize());

    REQUIRE(client.start_session("review this function"));
    REQUIRE(client.session_active());

    client.stop_session();
    REQUIRE_FALSE(client.session_active());
}

TEST_CASE("coding assistant lifecycle is idempotent", "[agent][baseline]") {
    agentforge::HydraForgeClient client;
    REQUIRE(client.initialize());

    client.start_session("task one");
    client.stop_session();
    client.start_session("task two");
    REQUIRE(client.session_active());

    client.stop_session();
    REQUIRE_FALSE(client.session_active());
}
```

**Step 3: 验证失败**

```bash
cd /workspace/project/AgentForge
cmake -B build/tests -S . -DAGENTFORGE_BUILD_TESTS=ON
cmake --build build/tests --target agentforge_tests -j"$(nproc)"
ctest --test-dir build/tests -R coding_assistant --output-on-failure
```

**预期**: 失败（`initialize()` 返回 `false`）。

### Task 2.2：接入最小 HydraForge Runtime/PDK

**Files**:
- Modify: `/workspace/project/AgentForge/CMakeLists.txt`

**要求**:
- 固定 HydraForge commit（不用漂移的 `GIT_TAG main`）
- 链接 `agenticdsl::engine` 或对应 Runtime target
- 链接 `hydraforge_pdk` INTERFACE target
- 保留 FTXUI 为非必要依赖
- 添加 Catch2 find_package

**约束**:
- 不修改 HydraForge 源码
- 不引入 OpenAI HTTP
- 不引入 MCP
- 不引入 PluginLoader

**Step 1: 修改 FetchContent pin commit**

```cmake
FetchContent_Declare(
    HydraForge
    GIT_REPOSITORY https://github.com/chisuhua/HydraForge.git
    GIT_TAG        <pinned-commit-hash>  # 实施时通过 sync-pdk.sh 输出获取
    GIT_SHALLOW    TRUE
)
```

**Step 2: 链接 Runtime + PDK targets**

```cmake
target_link_libraries(agentforge
    PRIVATE
        agenticdsl::engine
        hydraforge_pdk
)
```

**Step 3: 验证构建**

```bash
cmake -B build/tests -S . \
    -DAGENTFORGE_FETCH_HYDRAFORGE=ON \
    -DAGENTFORGE_BUILD_TESTS=ON
cmake --build build/tests -j"$(nproc)"
```

**Acceptance**:
- Configure 通过
- Build 通过（或只剩 stub 实现引起的 link 错误，可在 Task 2.3 修复）

### Task 2.3：实现 `coding_assistant` 最小路径

**Files**:
- Create: `/workspace/project/AgentForge/include/agentforge/agents/coding_assistant.h`
- Create: `/workspace/project/AgentForge/src/agents/coding_assistant.cpp`
- Modify: `/workspace/project/AgentForge/src/hydraforge_client.cpp`
- Modify: `/workspace/project/AgentForge/include/agentforge/hydraforge_client.h`

**Step 1: coding_assistant.h 公共接口**

```cpp
#ifndef AGENTFORGE_AGENTS_CODING_ASSISTANT_H
#define AGENTFORGE_AGENTS_CODING_ASSISTANT_H

#include <string>

namespace agentforge {

struct CodingRequest {
    std::string task;       // 用户任务描述（如 "review this function"）
    std::string code;       // 待审查代码（可选）
};

struct CodingResult {
    bool success = false;
    std::string summary;     // Mock 输出：固定字符串
    std::string error;
};

class CodingAssistantAgent {
public:
    static constexpr const char* kAgentId = "coding_assistant";

    CodingResult run(const CodingRequest& request) const;
};

}  // namespace agentforge

#endif
```

**Step 2: coding_assistant.cpp Mock 实现**

```cpp
#include "agentforge/agents/coding_assistant.h"

namespace agentforge {

CodingResult CodingAssistantAgent::run(const CodingRequest& request) const {
    if (request.task.empty()) {
        return CodingResult{
            .success = false,
            .summary = {},
            .error = "Missing arg: task",
        };
    }

    // MockLLM 路径：确定性输出
    return CodingResult{
        .success = true,
        .summary = "[coding_assistant mock] reviewed: " + request.task,
        .error = {},
    };
}

}  // namespace agentforge
```

**Step 3: HydraForgeClient::initialize() 实现最小 wire-up**

```cpp
// In src/hydraforge_client.cpp
bool HydraForgeClient::initialize() {
    std::lock_guard<std::mutex> lock(impl_->lifecycle_mutex_);
    if (impl_->initialized_.load()) {
        return true;
    }

    // 最小 wire-up：注册第一个 Agent 的最小入口
    // （不构建 DSLEngine, 不创建 InMemoryBus — U4 仅验证 Agent 存在）
    impl_->agents_.clear();
    impl_->agents_.emplace(
        CodingAssistantAgent::kAgentId,
        std::make_unique<CodingAssistantAgent>()
    );
    impl_->initialized_.store(true);
    return true;
}
```

**Step 4: HydraForgeClient 扩展 Agent 注册 API**

```cpp
// In include/agentforge/hydraforge_client.h
#include <memory>
#include <string>
#include <unordered_map>

namespace agenticdsl { class DSLEngine; }
namespace agentforge { class ICodingAgent; }

namespace agentforge {

class HydraForgeClient {
public:
    // ... existing API ...

    // Agent discovery
    bool has_agent(const std::string& agent_id) const;
    size_t agent_count() const;
    std::vector<std::string> list_agents() const;

    // Agent execution (returns JSON-serializable result for testability)
    std::string run_agent(const std::string& agent_id, const std::string& input);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace agentforge
```

**Step 5: 验证测试通过**

```bash
cmake --build build/tests --target agentforge_tests -j"$(nproc)"
ctest --test-dir build/tests -R coding_assistant --output-on-failure
```

**预期**: 3/3 测试通过。

### Task 2.4：阶段一收官验证

```bash
cd /workspace/project/AgentForge
cmake -B build/tests -S . \
    -DAGENTFORGE_FETCH_HYDRAFORGE=ON \
    -DAGENTFORGE_BUILD_TESTS=ON
cmake --build build/tests -j"$(nproc)"
ctest --test-dir build/tests --output-on-failure
```

**阶段一完成标准**:
- [ ] `coding_assistant` 有明确源文件
- [ ] `coding_assistant` 测试 3/3 PASS
- [ ] AgentForge 测试目标可独立运行
- [ ] Mock 模式可初始化
- [ ] HydraForge 代码无修改
- [ ] initialize/start/stop 生命周期可重复验证
- [ ] U4 diff 不含 `.mcp.json`/`.omo/`/`.opencode.json`/`AGENTS.md`

---

## 5. 阶段三：`doc_writer` 契约 + 失败测试

### Task 3.1：定义 `doc_writer` 公共接口

**Files**:
- Create: `/workspace/project/AgentForge/include/agentforge/agents/doc_writer.h`

```cpp
#ifndef AGENTFORGE_AGENTS_DOC_WRITER_H
#define AGENTFORGE_AGENTS_DOC_WRITER_H

#include <string>

namespace agentforge {

struct DocumentRequest {
    std::string request;     // 必填
    std::string context;     // 可选
    std::string format;      // 默认 "markdown"
};

struct DocumentResult {
    bool success = false;
    std::string agent;       // "doc_writer"
    std::string format;      // 实际使用的 format
    std::string content;
    std::string error;
};

class DocWriterAgent {
public:
    static constexpr const char* kAgentId = "doc_writer";

    DocumentResult run(const DocumentRequest& request) const;
};

}  // namespace agentforge

#endif
```

### Task 3.2：写失败测试

**Files**:
- Create: `/workspace/project/AgentForge/tests/test_doc_writer.cpp`

```cpp
#include <catch2/catch_test_macros.hpp>
#include "agentforge/agents/doc_writer.h"

using agentforge::DocumentRequest;
using agentforge::DocumentResult;
using agentforge::DocWriterAgent;

DocumentResult run_doc_writer(const DocumentRequest& req) {
    DocWriterAgent agent;
    return agent.run(req);
}

TEST_CASE("doc writer creates markdown from a request", "[agent][u4]") {
    auto result = run_doc_writer({
        .request = "Describe the public API",
        .context = "HydraForgeClient exposes initialize and stop_session",
        .format = "markdown",
    });

    REQUIRE(result.success);
    REQUIRE(result.agent == "doc_writer");
    REQUIRE(result.format == "markdown");
    REQUIRE_FALSE(result.content.empty());
}

TEST_CASE("doc writer defaults to markdown when format is empty", "[agent][u4]") {
    auto result = run_doc_writer({
        .request = "Write a release note",
        .context = "",
        .format = "",
    });

    REQUIRE(result.success);
    REQUIRE(result.format == "markdown");
}

TEST_CASE("doc writer rejects missing request", "[agent][u4]") {
    auto result = run_doc_writer({
        .request = "",
        .context = "context",
        .format = "markdown",
    });

    REQUIRE_FALSE(result.success);
    REQUIRE(result.error == "Missing arg: request");
}

TEST_CASE("doc writer does not invoke coding assistant", "[agent][u4]") {
    // 隔离测试：doc_writer 的失败/成功都不应影响 coding_assistant 的注册
    auto docs = run_doc_writer({.request = "", .context = "", .format = ""});
    REQUIRE_FALSE(docs.success);

    // coding_assistant 仍然可用
    agentforge::CodingAssistantAgent coding;
    auto coding_result = coding.run({.task = "noop", .code = ""});
    REQUIRE(coding_result.success);
}
```

**Step 3: 验证失败**

```bash
cd /workspace/project/AgentForge
cmake --build build/tests --target agentforge_tests -j"$(nproc)"
ctest --test-dir build/tests -R doc_writer --output-on-failure
```

**预期**: 失败（`DocWriterAgent` 类不存在）。

---

## 6. 阶段四：`doc_writer` 实现

### Task 4.1：实现 `DocWriterAgent`

**Files**:
- Create: `/workspace/project/AgentForge/src/agents/doc_writer.cpp`

```cpp
#include "agentforge/agents/doc_writer.h"

namespace agentforge {

namespace {

std::string normalize_format(const std::string& format) {
    return format.empty() ? "markdown" : format;
}

std::string render_markdown(const DocumentRequest& req) {
    std::string out;
    out += "# Generated Document\n\n";
    out += "Request: " + req.request + "\n\n";
    if (!req.context.empty()) {
        out += "## Context\n\n";
        out += req.context + "\n";
    }
    return out;
}

}  // namespace

DocumentResult DocWriterAgent::run(const DocumentRequest& request) const {
    if (request.request.empty()) {
        return DocumentResult{
            .success = false,
            .agent = kAgentId,
            .format = normalize_format(request.format),
            .content = {},
            .error = "Missing arg: request",
        };
    }

    const auto format = normalize_format(request.format);

    // MockLLM 路径：确定性输出
    // 真实 LLM 仅通过 HydraForge ILLMProvider 抽象调用（不在 U4 范围）
    return DocumentResult{
        .success = true,
        .agent = kAgentId,
        .format = format,
        .content = render_markdown(request),
        .error = {},
    };
}

}  // namespace agentforge
```

### Task 4.2：CMake 添加新源文件

**Files**:
- Modify: `/workspace/project/AgentForge/CMakeLists.txt`

```cmake
add_executable(agentforge
    src/main.cpp
    src/hydraforge_client.cpp
    src/agents/coding_assistant.cpp    # 阶段二新增
    src/agents/doc_writer.cpp          # 阶段四新增
)
```

### Task 4.3：接入 HydraForgeClient 双 Agent 路由

**Files**:
- Modify: `/workspace/project/AgentForge/src/hydraforge_client.cpp`

```cpp
bool HydraForgeClient::initialize() {
    std::lock_guard<std::mutex> lock(impl_->lifecycle_mutex_);
    if (impl_->initialized_.load()) {
        return true;
    }

    impl_->agents_.clear();
    impl_->agents_.emplace(
        CodingAssistantAgent::kAgentId,
        std::make_unique<CodingAssistantAgent>()
    );
    impl_->agents_.emplace(
        DocWriterAgent::kAgentId,
        std::make_unique<DocWriterAgent>()
    );
    impl_->initialized_.store(true);
    return true;
}

bool HydraForgeClient::has_agent(const std::string& agent_id) const {
    if (!impl_) return false;
    std::lock_guard<std::mutex> lock(impl_->lifecycle_mutex_);
    return impl_->agents_.count(agent_id) > 0;
}

size_t HydraForgeClient::agent_count() const {
    if (!impl_) return 0;
    std::lock_guard<std::mutex> lock(impl_->lifecycle_mutex_);
    return impl_->agents_.size();
}

std::vector<std::string> HydraForgeClient::list_agents() const {
    std::vector<std::string> ids;
    if (!impl_) return ids;
    std::lock_guard<std::mutex> lock(impl_->lifecycle_mutex_);
    ids.reserve(impl_->agents_.size());
    for (const auto& [id, _] : impl_->agents_) {
        ids.push_back(id);
    }
    return ids;
}

std::string HydraForgeClient::run_agent(const std::string& agent_id, const std::string& input) {
    if (!impl_) return R"({"success":false,"error":"client not initialized"})";
    std::lock_guard<std::mutex> lock(impl_->lifecycle_mutex_);

    auto it = impl_->agents_.find(agent_id);
    if (it == impl_->agents_.end()) {
        return R"({"success":false,"error":"unknown agent"})";
    }

    // U4 仅 Mock 路径：JSON-shaped 输出
    if (agent_id == CodingAssistantAgent::kAgentId) {
        CodingRequest req{.task = input, .code = {}};
        auto result = static_cast<CodingAssistantAgent*>(it->second.get())->run(req);
        // 简单 JSON 序列化（U4 范围内）
        return std::string("{\"success\":") + (result.success ? "true" : "false") +
               ",\"summary\":\"" + result.summary + "\",\"error\":\"" + result.error + "\"}";
    }

    if (agent_id == DocWriterAgent::kAgentId) {
        DocumentRequest req{.request = input, .context = {}, .format = {}};
        auto result = static_cast<DocWriterAgent*>(it->second.get())->run(req);
        return std::string("{\"success\":") + (result.success ? "true" : "false") +
               ",\"agent\":\"doc_writer\",\"format\":\"" + result.format +
               "\",\"error\":\"" + result.error + "\"}";
    }

    return R"({"success":false,"error":"unsupported agent"})";
}
```

### Task 4.4：阶段四验证

```bash
cd /workspace/project/AgentForge
cmake --build build/tests -j"$(nproc)"
ctest --test-dir build/tests -R doc_writer --output-on-failure
```

**预期**: 4/4 测试通过。

---

## 7. 阶段五：双 Agent 集成测试

### Task 5.1：双 Agent 集成测试

**Files**:
- Create: `/workspace/project/AgentForge/tests/test_two_agents.cpp`

```cpp
#include <catch2/catch_test_macros.hpp>
#include "agentforge/hydraforge_client.h"

TEST_CASE("AgentForge exposes two independent domain agents", "[u4][integration]") {
    agentforge::HydraForgeClient client;
    REQUIRE(client.initialize());

    REQUIRE(client.has_agent("coding_assistant"));
    REQUIRE(client.has_agent("doc_writer"));
    REQUIRE(client.agent_count() == 2);

    auto agents = client.list_agents();
    REQUIRE(agents.size() == 2);
}

TEST_CASE("two agents can run independently", "[u4][integration]") {
    agentforge::HydraForgeClient client;
    REQUIRE(client.initialize());

    auto coding_output = client.run_agent("coding_assistant", "review int add(int, int)");
    REQUIRE(coding_output.find("\"success\":true") != std::string::npos);

    auto docs_output = client.run_agent("doc_writer", "document int add(int, int)");
    REQUIRE(docs_output.find("\"success\":true") != std::string::npos);
}

TEST_CASE("agent failure does not unregister another agent", "[u4][integration]") {
    agentforge::HydraForgeClient client;
    REQUIRE(client.initialize());

    // doc_writer 失败
    auto failed = client.run_agent("doc_writer", "");
    REQUIRE(failed.find("\"success\":false") != std::string::npos);

    // coding_assistant 仍然可用
    REQUIRE(client.has_agent("coding_assistant"));
    auto coding = client.run_agent("coding_assistant", "noop");
    REQUIRE(coding.find("\"success\":true") != std::string::npos);
}

TEST_CASE("unknown agent returns failure without unregistering known agents", "[u4][integration]") {
    agentforge::HydraForgeClient client;
    REQUIRE(client.initialize());

    auto unknown = client.run_agent("nonexistent_agent", "test");
    REQUIRE(unknown.find("\"success\":false") != std::string::npos);

    REQUIRE(client.has_agent("coding_assistant"));
    REQUIRE(client.has_agent("doc_writer"));
}
```

**Step 2: 验证**

```bash
cd /workspace/project/AgentForge
cmake --build build/tests -j"$(nproc)"
ctest --test-dir build/tests -R 'two_agents|integration' --output-on-failure
```

**预期**: 4/4 测试通过。

### Task 5.2：HydraForge 端零回归验证

```bash
cd /workspace/project/HydraForge
cmake --build build/tests -j"$(nproc)"
ctest --test-dir build/tests --output-on-failure | tail -10
```

**Acceptance**:
- HydraForge 测试基线保持 ≥ 184/185（无新增失败）
- U4 不修改 HydraForge 源码

---

## 8. 阶段六：最终验证与文档同步

### Task 6.1：全量构建与测试

```bash
cd /workspace/project/AgentForge

cmake -B build/tests -S . \
    -DAGENTFORGE_FETCH_HYDRAFORGE=ON \
    -DAGENTFORGE_BUILD_TESTS=ON

cmake --build build/tests -j"$(nproc)"

ctest --test-dir build/tests --output-on-failure
```

**Acceptance**:
- Configure 通过
- Build 通过
- CTest 通过率 ≥ 11/11（coding_assistant 3 + doc_writer 4 + integration 4）

### Task 6.2：LSP diagnostics

```bash
# 对 changed files 检查
lsp_diagnostics /workspace/project/AgentForge/include/agentforge/agents/coding_assistant.h
lsp_diagnostics /workspace/project/AgentForge/include/agentforge/agents/doc_writer.h
lsp_diagnostics /workspace/project/AgentForge/src/agents/coding_assistant.cpp
lsp_diagnostics /workspace/project/AgentForge/src/agents/doc_writer.cpp
lsp_diagnostics /workspace/project/AgentForge/src/hydraforge_client.cpp
lsp_diagnostics /workspace/project/AgentForge/tests/test_coding_assistant.cpp
lsp_diagnostics /workspace/project/AgentForge/tests/test_doc_writer.cpp
lsp_diagnostics /workspace/project/AgentForge/tests/test_two_agents.cpp
```

**Acceptance**:
- changed files 无 error
- 无 `as any` / `-Wno-*` / 警告压制

### Task 6.3：AgentForge 文档同步

**Files**:
- Modify: `/workspace/project/AgentForge/README.md`
- Modify: `/workspace/project/AgentForge/docs/ADR-AF-001-design.md`

**README.md 更新**:

```markdown
## 当前状态 (2026-09-03)

- ✅ Sprint 24 Day 1-2 bootstrap skeleton (CMake + FTXUI vendor + HydraForgeClient interface)
- ✅ **Sprint 25+ U4: dual-domain agent base** — `coding_assistant` + `doc_writer` 共存
- ✅ MockLLM 驱动的双 Agent 集成测试通过（11 cases）
- 🟡 Day 3+ TUI/真实 LLM 集成仍 deferred（per `docs/ADR-AF-001-design.md` Known Limitations）
```

**ADR-AF-001-design.md 更新**:

新增 §决策 6: 双 Agent 路由策略

```markdown
### 决策 6: 双 Agent 路由 (2026-09-03 Sprint 25+ U4)

| 选项 | 接受/拒绝 | 理由 |
|------|:---------:|------|
| **Client 内部 `std::unordered_map<std::string, std::unique_ptr<IAgent>>`** | ✅ 接受 | U4 仅 2 个 Agent，map 足够；`agentforge::IAgent` 抽象最小化（virtual ~`run(const std::string&)`）；未来扩展 Registry 再升级 |
| HydraForge `IAgentRegistry` (ADR-0082 V1 骨架) | ❌ (U4) | V1 骨架 ship 但 AgentForge 前置阶段未接入；U4 范围内避免双引入 |
| CapabilityRegistry + RemoteAgentAdapter | ❌ (U4) | ADR-0060 决策 2，Phase 2 目标 |

**演进路径**: Sprint 28+（AgentForge Phase 3）接入 `IAgentRegistry`，将 map 替换为 `IAgentRegistry::register_agent()`。
```

### Task 6.4：HydraForge 端文档同步

**Files**:
- Modify: `/workspace/project/HydraForge/docs/active-status.md`（移除 U4 carry-over 行）
- Modify: `/workspace/project/HydraForge/roadmap.md`（更新 Phase 7a 启动条件 C1）

**active-status.md 更新**:

```markdown
- U4 (AgentForge 第 2 个领域 agent, 8h, P0 — Phase 7a C1 唯一代码解锁项) — ✅ closed 2026-09-XX
```

**roadmap.md 更新**:

Phase 7 启动条件表 C1 行更新：

```text
| AgentForge 第 2 agent | 可独立运行 | ✅ doc_writer 已 ship (U4 2026-09-XX) | ✅ PASS | — |
```

---

## 9. U4 验收标准

### 必须满足（Ship Gate）

- [ ] AgentForge 拥有实际可编译、可测试的 `coding_assistant` 第一 Agent
- [ ] 第一 Agent 有 Mock 驱动测试 ≥ 3 cases
- [ ] 新增 `doc_writer` 第二领域 Agent
- [ ] 两个 Agent 使用同一套 HydraForge PDK 接入路径
- [ ] 两个 Agent 可以同时注册并被发现
- [ ] 两个 Agent 可以独立调用
- [ ] `doc_writer` 正常请求通过
- [ ] `doc_writer` 缺少 `request` 返回结构化失败
- [ ] `doc_writer` `format` 缺失时默认为 markdown
- [ ] 一个 Agent 失败不会注销或破坏另一个 Agent
- [ ] 普通测试不依赖真实 API Key
- [ ] AgentForge CMake configure/build 通过
- [ ] AgentForge CTest 通过 ≥ 11/11
- [ ] changed files LSP diagnostics clean
- [ ] HydraForge 主仓库零代码修改
- [ ] HydraForge 测试基线 ≥ 184/185 保持
- [ ] U4 变更不引入 submodule / monorepo 内嵌
- [ ] U4 diff 不含 `.mcp.json`/`.omo/`/`.opencode.json`/`AGENTS.md`

### 不应作为 U4 验收条件

- 真实 OpenAI E2E 成功
- 多 panel FTXUI TUI
- streaming token 渲染
- SQL 数据库连通
- MCP Server
- 云服务部署
- Agent-to-Agent 委派
- PlanExecute / ForkJoin loop
- 完整 7 工具集
- Session 持久化
- 用户插件加载

---

## 10. 提交策略

AgentForge 是独立 repo，建议 3 个原子 commit（dual-repo 工作流下 HydraForge 端可能另起 1 个 sync commit）：

```text
1. feat(baseline): wire minimal coding assistant test path
   - HydraForgeClient::initialize() 真实实现（Mock）
   - coding_assistant.{h,cpp} + tests/test_coding_assistant.cpp
   - CMakeLists.txt 添加测试目标 + Catch2 find_package

2. feat(u4): add doc_writer domain agent
   - doc_writer.{h,cpp}
   - HydraForgeClient 双 Agent 路由
   - tests/test_doc_writer.cpp

3. test(u4): verify independent dual-agent execution
   - tests/test_two_agents.cpp
   - 文档更新（README + ADR-AF-001 §决策 6）

4. docs(sync): [HydraForge] update Phase 7a C1 to PASS + active-status U4 closed
```

每个 commit 前执行：

```bash
cd /workspace/project/AgentForge
git diff --check
cmake --build build/tests -j"$(nproc)"
ctest --test-dir build/tests --output-on-failure
```

提交信息遵循 HydraForge repo 既有风格（SEMANTIC: `feat:` / `test:` / `docs:`，中文 body 允许）。

---

## 11. 依赖图

```text
阶段一 (dual-repo 同步)
  ├─ Task 1.1 PDK 同步
  ├─ Task 1.2 HydraForge 基线
  └─ Task 1.3 AgentForge skeleton build
    ↓
阶段二 (第一 Agent 基线，前置)
  ├─ Task 2.1 测试框架 + 失败测试
  ├─ Task 2.2 HydraForge Runtime/PDK 接入
  └─ Task 2.3 coding_assistant 最小实现
    ↓
阶段三 (第二 Agent 契约)
  ├─ Task 3.1 doc_writer.h 公共接口
  └─ Task 3.2 失败测试
    ↓
阶段四 (第二 Agent 实现)
  ├─ Task 4.1 DocWriterAgent 实现
  ├─ Task 4.2 CMake 添加
  └─ Task 4.3 Client 双 Agent 路由
    ↓
阶段五 (双 Agent 集成)
  ├─ Task 5.1 双 Agent 测试
  └─ Task 5.2 HydraForge 零回归
    ↓
阶段六 (Ship Gate)
  ├─ Task 6.1 全量 build + ctest
  ├─ Task 6.2 LSP diagnostics
  ├─ Task 6.3 AgentForge 文档
  └─ Task 6.4 HydraForge 文档
    ↓
U4 ship
```

## 12. 风险与缓解

### 12.1 高风险：U4 名称与真实状态不匹配

**问题**: 路线图称 "第二 Agent"，但第一 Agent 不存在。

**缓解**:
- 本计划明确分为 "前置基线 + U4 核心"
- 不把 HydraForge G1 插件冒充 AgentForge 第一 Agent
- 验收必须包含第一 Agent 的文件 + 测试证据

### 12.2 高风险：HydraForge FetchContent / Runtime target 依赖不稳定

**问题**: 当前 AgentForge CMake 使用 `GIT_TAG main`，且默认 FetchContent OFF。

**缓解**:
- 实施前 pin HydraForge commit（通过 `scripts/sync-pdk.sh` 输出）
- 不用漂移的 main
- FetchContent 阻塞时记录外部依赖风险，不改做 MCP/HTTP fallback

### 12.3 中风险：蓝图与当前主线漂移

**问题**: Blueprint 创建于 2026-07-15，当前 HydraForge 已 ship `IAgentRegistry` V1、Agent hook、Composition 契约等。

**缓解**:
- 只采用当前实际可编译、可测试的 HydraForge API
- 不根据 README/ADR 历史描述猜测 target
- 每个外部 API 用当前头文件 + 最小编译测试确认
- U4 仅依赖 React + 基础 registry/handler，不依赖 Agent hook/composition

### 12.4 中风险：第二 Agent 扩成多 Agent 平台

**缓解**（硬性排除）:
- Agent-to-Agent delegation
- ForkJoin / PlanExecute
- 动态 plugin loading
- MCP / 服务化 / 云部署
- 跨进程通信

### 12.5 中风险：真实 LLM 凭据污染测试

**缓解**:
- 默认所有 U4 测试使用 Mock
- 不引入 OpenAI key 依赖
- 不在 U4 契约中写入模型/provider/部署信息

### 12.6 低风险：AgentForge 工作区 untracked 工具文件污染 U4 diff

**缓解**:
- Task 1.3 显式记录 `.mcp.json` / `.omo/` / `.opencode.json` / `AGENTS.md` 不纳入 U4
- 提交前 `git status --short` 检查
- `.gitignore` 修改需单独评估（如果合理可纳入基线 commit 1）

---

## 13. 关联文档

| 文档 | 关联 |
|---|---|
| [docs/superpowers/plans/2026-07-15-phase6-agentforge-mvp.md](./2026-07-15-phase6-agentforge-mvp.md) | Sprint 24 总体计划 |
| [docs/proposals/implementation/agentforge-mvp-blueprint.md](../proposals/implementation/agentforge-mvp-blueprint.md) | AgentForge 蓝图设计 |
| [HydraForge `pdk/g1_coding_assistant/`](../pdk/g1_coding_assistant/) | React loop 参考插件 |
| [HydraForge `pdk/g3_knowledge_base/`](../pdk/g3_knowledge_base/) | `register_tool_function` + SessionStore 范式参考 |
| [HydraForge `examples/agent_simple/`](../examples/agent_simple/) | MockLLMProvider 单次 run 入门模板 |
| [HydraForge `examples/phase5_yield_token_generator/`](../examples/phase5_yield_token_generator/) | YIELD/STREAM token 流范式（U4 不使用） |
| `scripts/sync-pdk.sh` | Dual-Repo 同步工具 |

---

## 14. 决策日志

| 日期 | 决策 | 依据 |
|---|---|---|
| 2026-09-03 | Repo 关系 = 维持 dual-repo（不引入 submodule / monorepo） | ADR-0021 §7 + 蓝图 §二 + scripts/sync-pdk.sh 已 ship |
| 2026-09-03 | 第二 Agent = `doc_writer`（非 `sql_assistant`） | 8h U4 范围内最小依赖、易确定性验证、避免数据库集成 |
| 2026-09-03 | 双 Agent 路由 = Client 内部 `unordered_map`（非 `IAgentRegistry`） | U4 仅 2 个 Agent；最小抽象避免双引入；演进路径明确 |
| 2026-09-03 | U4 范围 = 前置基线（第一 Agent）+ U4 核心（第二 Agent + 集成） | 路线图假设第一 Agent 存在，但本地代码事实不符 |
| 2026-09-03 | U4 不实施 Agent-to-Agent / ForkJoin / MCP / 真实云 LLM | 硬性 out-of-scope（避免范围蔓延） |

---

## 15. Ship Gate 检查清单（实施前 + 实施后）

### 实施前确认

- [ ] 本计划用户已批准
- [ ] HydraForge commit pin 已确定
- [ ] AgentForge 工作区 untracked 工具文件不纳入
- [ ] User 已确认使用 `git worktree` 隔离（推荐）或本地直接实施

### 实施后确认（每 commit）

- [ ] `git diff --check` 无 whitespace 错误
- [ ] `cmake --build build/tests` 通过
- [ ] `ctest --test-dir build/tests --output-on-failure` 全绿
- [ ] HydraForge 端 `ctest` 基线保持
- [ ] changed files `lsp_diagnostics` clean

### Ship Gate（最终）

- [ ] 所有 "必须满足" 项勾选
- [ ] 11+ 测试通过
- [ ] HydraForge 端零回归
- [ ] AgentForge README + ADR-AF-001 + active-status + roadmap 同步
- [ ] 3 个 atomic commit 已推送（如有远程）

---

**最后更新**: 2026-09-03（基于 AgentForge 本地代码侦察 + HydraForge 路线图 U4 任务定义）
**计划状态**: ✅ **Shipped (2026-09-03)** — AgentForge 3 atomic commits + HydraForge 端 carry-over 闭环
**实施记录**:
- `dfc6882` (AgentForge): `refactor(build): extract agentforge_core library + pin HydraForge f106c97`
- `7b4330c` (AgentForge): `feat(agents): add coding_assistant + doc_writer domain agents` (11/11 tests, 34 assertions)
- `2e10104` (AgentForge): `docs(adr+readme): add §决策 6/7 to ADR-AF-001 + U4 status to README`
- HydraForge 端 zero code change（验证 PDK 复用性的跨 repo 物理隔离）; `active-status.md` + `roadmap.md` carry-over 闭环
- Phase 7a C1: ❌ FAIL → ✅ **PASS** (AgentForge ≥2 agents)
- Phase 7a 启动复评：仍 gated (C2 Solo Dev 1 人 + C5 Evidence Gate Conditional)
**下一决策点**: Phase 7a 启动复评（每 Sprint 收官重跑 `scripts/control-plane-eval.py --relaxed`）
