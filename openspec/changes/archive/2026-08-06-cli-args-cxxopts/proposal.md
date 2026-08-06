# cli-args-cxxopts

## Why

- `main.cpp:76-83` 手撸 args 循环仅支持 `--mock` 和 `--session <id>`，pi-agent CLI flag 设计（§九）要求的 `-p` print 模式 / `--mode json|rpc` / `-c` 续最近 / `-r` 选择 / `--provider` / `--offline` 全部缺位。
- 手撸解析已成为新增 flag 的摩擦点（chat-streaming-slash-tui 需要新增 2 个 flag，后续 Wave 3 还有更多）。
- 缺失能力分析定性：3 天工作量，cxxopts/argparse 引入。

## What Changes

**In Scope**:

- (TBD)

### 关键场景

- GIVEN 任意既有命令行（`--mock`、`--session <id>`、组合），WHEN 迁移后，THEN 行为与迁移前完全一致。
- GIVEN `--help`，WHEN 执行，THEN 输出全部 flag 声明与说明（集中化生成，非手写字符串拼接）。
- GIVEN 未知 flag，WHEN 解析，THEN 报错并提示 `--help`，exit code 非零。

**Out of Scope**:

- (TBD)

## Capabilities

- MUST cxxopts vendored 到 external/（与 nlohmann_json/inja 同一惯例），禁止系统包依赖。
- MUST 既有 flag 行为等价（E2E 回归）。
- SHOULD flag 声明表数据驱动（新 flag 只加一行声明）。

## Impact

- MUST cxxopts vendored 到 external/（与 nlohmann_json/inja 同一惯例），禁止系统包依赖。
- MUST 既有 flag 行为等价（E2E 回归）。
- SHOULD flag 声明表数据驱动（新 flag 只加一行声明）。

## Acceptance

- 既有 flag 组合 E2E 回归通过；`--help` 集中化输出。
- 新增 3 个 flag 生效；ctest 全量零回归。

