# Sync PDK Contract Header — Tasks

## 1. Setup / 准备
- [ ] 1.1 枚举 `include/agenticdsl/pdk/` 下全部头文件的 `<agenticdsl/contract/...>` include 引用 (grep)，确认 PDK_CONTRACT_DEPS 清单
- [ ] 1.2 确认 standalone 仓库 target_include_directories 已含 `include/`（contract 头路径解析）

## 2. RED — 测试先行
- [ ] 2.1 新增 `tests/test_sync_pdk_contract.sh`: 断言 `PDK_SYNC_DRY_RUN=1 scripts/sync-pdk.sh` 输出含 itool_registry.h + 工作目录文件存在
- [ ] 2.2 运行 RED: 确认测试 FAIL (当前 sync-pdk.sh 无 contract 逻辑)

## 3. GREEN — 实现
- [ ] 3.1 `sync-pdk.sh`: 新增 `PDK_CONTRACT_DEPS` 数组 (itool_registry.h + iinteraction_bus.h + timer_service.h + ilogger.h)
- [ ] 3.2 `sync-pdk.sh`: 新增 contract 同步段 — 对每个 dep: mkdir -p + cp 到 `${WORK_DIR}/include/agenticdsl/contract/`
- [ ] 3.3 `sync-pdk.sh`: DRY_RUN 模式输出 contract 清单 (log_ok "Copied N contract headers: ...")
- [ ] 3.4 运行 GREEN: 测试 PASS

## 4. REFACTOR / 收尾
- [ ] 4.1 检查既有 sync-pdk.sh preflight/copy/commit 结构未被破坏 (非 contract 段零变更)
- [ ] 4.2 注释更新: sync-pdk.sh 头注释 (contract 同步段说明)
- [ ] 4.3 shellcheck 或 bash -n 语法校验

## 5. VERIFY — 验证
- [ ] 5.1 `PDK_SYNC_DRY_RUN=1 scripts/sync-pdk.sh` 实际运行 → 退出 0 + contract 清单输出
- [ ] 5.2 确认 dry-run 工作目录 `include/agenticdsl/contract/itool_registry.h` 存在
- [ ] 5.3 全量 ctest 零回归 (脚本变更不影响 C++ 测试)
- [ ] 5.4 `openspec validate sync-pdk-contract-header --strict` → "Change is valid"

## 6. DOCS / 治理
- [ ] 6.1 Oracle bg_5db13fe0 审查闭环标注: "DB1 PDK 生态影响 ✅ resolved by sync-pdk-contract-header"
- [ ] 6.2 Roadmap: Pre-Wave3 收口门禁 checklist 项 (3) 标 ✅
- [ ] 6.3 `docs/active-status.md` + master plan 同步 (change ship + archive)
- [ ] 6.4 archive change (4 文件完整 per AGENTS.md Day 5 lesson)
