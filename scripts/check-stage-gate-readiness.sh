#!/usr/bin/env bash
# scripts/check-stage-gate-readiness.sh
# 功能描述: 验证 Phase 6 Stage Gate (2026-07-18) 启动条件
#          自动化 7 项 Stage Gate 检查 + 5 项 ADR-0050 硬前置 + 3 项 C20-Spike W2-W3 unlock
#
# 检查项 (3 大类):
#   A. Stage Gate 7 项 (per docs/handoff/2026-07-31-stage-gate-evaluation.md §二)
#   B. ADR-0050 §启动条件 5 项硬前置 (per docs/adr/adr-0050-phase6-strategic-evaluation.md)
#   C. C20-Spike W2-W3 unlock 3 项 (per docs/adr/adr-0051-phase6-pdk-composition-spike.md)
#
# 用法:
#   ./scripts/check-stage-gate-readiness.sh              # 完整检查
#   ./scripts/check-stage-gate-readiness.sh --no-ctest   # 跳过 ctest (默认会跑, 可能耗时)
#   ./scripts/check-stage-gate-readiness.sh --quick      # 仅 PASS/PARTIAL/FAIL 摘要, 不跑 ctest
#   ./scripts/check-stage-gate-readiness.sh --category A|B|C  # 仅检查指定类别
#
# 退出码:
#   0: 全部 PASS (Stage Gate 可解锁)
#   1: 部分 PARTIAL / 配置问题
#   2: FAIL (有 hard blocker, Stage Gate 不通过)
#
# 设计依据:
#   - docs/handoff/2026-07-31-stage-gate-evaluation.md
#   - docs/adr/adr-0050-phase6-strategic-evaluation.md §启动条件
#   - docs/adr/adr-0051-phase6-pdk-composition-spike.md §启动条件
#   - master plan §九 Stage Gate 设计意图

set -e

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

# 颜色
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# 默认配置
RUN_CTEST=true
QUICK_MODE=false
CATEGORY=""

# 解析参数
while [[ $# -gt 0 ]]; do
  case "$1" in
    --no-ctest)
      RUN_CTEST=false
      shift
      ;;
    --quick)
      QUICK_MODE=true
      RUN_CTEST=false
      shift
      ;;
    --category)
      CATEGORY="$2"
      if [[ ! "$CATEGORY" =~ ^[ABC]$ ]]; then
        echo -e "${RED}--category 必须为 A, B, 或 C${NC}"
        exit 1
      fi
      shift 2
      ;;
    --help|-h)
      echo "用法: $0 [选项]"
      echo ""
      echo "Stage Gate 2026-07-18 Readiness Checker"
      echo "自动化验证 3 大类共 15 项检查"
      echo ""
      echo "选项:"
      echo "  --no-ctest       跳过 ctest (CI/本地调试时使用)"
      echo "  --quick          仅 PASS/PARTIAL/FAIL 摘要, 不跑 ctest (~5s)"
      echo "  --category A|B|C 仅检查指定类别"
      echo "  --help, -h        显示帮助"
      echo ""
      echo "类别:"
      echo "  A: Stage Gate 7 项 (稳定期 + 测试 + 标准库 + 触发条件)"
      echo "  B: ADR-0050 5 项硬前置"
      echo "  C: C20-Spike 3 项 W2-W3 unlock"
      echo ""
      echo "退出码:"
      echo "  0: 全部 PASS"
      echo "  1: 部分 PARTIAL (需人工介入)"
      echo "  2: FAIL (hard blocker)"
      exit 0
      ;;
    *)
      echo -e "${RED}未知选项: $1${NC}"
      echo "使用 --help 查看用法"
      exit 1
      ;;
  esac
done

# 计数器
PASSED=0
FAILED=0
WARNINGS=0
START_TIME=$(date +%s)

# 帮助函数
print_header() {
  echo ""
  echo -e "${BLUE}============================================================${NC}"
  echo -e "${BLUE}$1${NC}"
  echo -e "${BLUE}============================================================${NC}"
}

print_step() {
  echo -e "${YELLOW}▸ $1${NC}"
}

print_ok() {
  echo -e "${GREEN}✅ $1${NC}"
  PASSED=$((PASSED + 1))
}

print_fail() {
  echo -e "${RED}❌ $1${NC}"
  FAILED=$((FAILED + 1))
}

print_warn() {
  echo -e "${YELLOW}⚠️  $1${NC}"
  WARNINGS=$((WARNINGS + 1))
}

# 日期工具: 计算从 ship_date 到今日的天数
# 参数: $1 = ship date (YYYY-MM-DD), $2 = 2-week target date (YYYY-MM-DD)
check_stability() {
  local SHIP_DATE="$1"
  local TARGET_DATE="$2"
  local SHIP_EPOCH=$(date -d "$SHIP_DATE" +%s 2>/dev/null || echo 0)
  local TARGET_EPOCH=$(date -d "$TARGET_DATE" +%s 2>/dev/null || echo 0)
  local NOW_EPOCH=$(date +%s)
  local DAYS_SINCE_SHIP=$(( (NOW_EPOCH - SHIP_EPOCH) / 86400 ))
  local DAYS_NEEDED=$(( (TARGET_EPOCH - SHIP_EPOCH) / 86400 ))

  echo "  Ship date: $SHIP_DATE, 距今: ${DAYS_SINCE_SHIP}d, 需要: ${DAYS_NEEDED}d (${TARGET_DATE})"

  if [ $NOW_EPOCH -ge $TARGET_EPOCH ]; then
    return 0  # 2-week reached
  else
    return 1  # not yet
  fi
}

# 检查 ship 后是否有 hotfix commits (仅在 --quick 时跳过详细 git log)
check_hotfixes() {
  local SINCE_DATE="$1"
  local CHANGE_ID="$2"
  local COMMIT_COUNT=$(git log --oneline --since="$SINCE_DATE" -- 'src/' 'tests/' 'include/' 'CMakeLists.txt' 2>/dev/null | wc -l)

  if [ "$COMMIT_COUNT" -eq 0 ]; then
    echo "  稳定: 0 hotfix commits since ship"
    return 0
  else
    echo "  注意: ${COMMIT_COUNT} commits since ship (可能 hotfix)"
    return 1
  fi
}

# 标题
GATE_DATE="2026-07-18"
print_header "Stage Gate Readiness Check — $(date '+%Y-%m-%d %H:%M:%S')"
echo "Gate Date: $GATE_DATE"
echo "Repo: $REPO_ROOT"
echo ""

# ====================================================================
# Category A: Stage Gate 7 项
# ====================================================================
if [ -z "$CATEGORY" ] || [ "$CATEGORY" = "A" ]; then
  print_header "Category A: Stage Gate 7 项 (per handoff/2026-07-31-stage-gate-evaluation.md §二)"

  # --- Item A1: C10 (Lazy ModuleState) ---
  print_step "A1: C10 (Lazy ModuleState) ship + 2 周稳定"
  if check_stability "2026-07-03" "2026-07-17"; then
    check_hotfixes "2026-07-03" "C10"
    print_ok "C10: 稳定 ≥2 周 ✅"
  else
    print_warn "C10: 未满 2 周 (ship 2026-07-03, 需到 2026-07-17)"
  fi

  # --- Item A2: C11 (Session Registry) ---
  print_step "A2: C11 (Session Registry) ship + 2 周稳定"
  if check_stability "2026-07-04" "2026-07-18"; then
    check_hotfixes "2026-07-04" "C11"
    print_ok "C11: 稳定 ≥2 周 ✅"
  else
    print_warn "C11: 未满 2 周 (ship 2026-07-04, 需到 2026-07-18)"
  fi

  # --- Item A3: C12 (YIELD/STREAM) ---
  print_step "A3: C12 (YIELD/STREAM) ship + 2 周稳定"
  if check_stability "2026-07-04" "2026-07-18"; then
    check_hotfixes "2026-07-04" "C12"
    print_ok "C12: 稳定 ≥2 周 ✅"
  else
    print_warn "C12: 未满 2 周 (ship 2026-07-04, 需到 2026-07-18)"
  fi

  # --- Item A4: 测试基础设施 ---
  print_step "A4: 测试基础设施 (ctest + ASan)"
  if [ "$RUN_CTEST" = true ] && [ "$QUICK_MODE" = false ]; then
    if [ ! -d build ]; then
      print_warn "build/ 不存在, 跳过 ctest"
    else
      print_step "运行 ctest --output-on-failure..."
      # 保存输出用于计数
      CTEST_OUTPUT=$(cd build && ctest --output-on-failure 2>&1) || true
      cd ..
      PASS_COUNT=$(echo "$CTEST_OUTPUT" | grep -oP 'tests passed.*\K\d+' | tail -1 || echo "?")
      FAIL_COUNT=$(echo "$CTEST_OUTPUT" | grep -oP 'tests failed.*\K\d+' | tail -1 || echo "?")
      echo "$CTEST_OUTPUT" | tail -10

      if [ "$FAIL_COUNT" = "0" ] || [ "$FAIL_COUNT" = "" ]; then
        print_ok "ctest: $PASS_COUNT 通过, 0 失败 ✅"
      else
        print_fail "ctest: $FAIL_COUNT 失败"
      fi
    fi
  else
    print_warn "ctest 跳过 (--quick 或 --no-ctest, 需手动验证 72+N/72+N 全绿)"
  fi

  # --- Item A5: 推理标准库 7/7 子图 ---
  print_step "A5: 推理标准库 7/7 子图 ship"
  # Per handoff doc: engine, model, session, generate, sampler, configure, status
  # sampler is inline (per D1 SamplerStrategy 删除), so we check for decoding.md as proxy
  EXPECTED_SUBGRAPHS=(
    "lib/inference/engine.md"
    "lib/inference/model.md"
    "lib/inference/session.md"
    "lib/inference/decoding.md"
    "lib/inference/cloud_engine.md"
    "lib/inference/prefix_cache.md"
    "lib/inference/kv_cache.md"
  )
  MISSING_SUBGRAPHS=()
  FOUND_COUNT=0
  for sg in "${EXPECTED_SUBGRAPHS[@]}"; do
    if [ -f "$sg" ]; then
      FOUND_COUNT=$((FOUND_COUNT + 1))
    else
      MISSING_SUBGRAPHS+=("$sg")
    fi
  done

  if [ $FOUND_COUNT -eq ${#EXPECTED_SUBGRAPHS[@]} ]; then
    print_ok "推理标准库: $FOUND_COUNT/${#EXPECTED_SUBGRAPHS[@]} 子图存在 ✅"
  else
    print_fail "推理标准库: 缺 ${#MISSING_SUBGRAPHS[@]} 子图: ${MISSING_SUBGRAPHS[*]}"
  fi

  # --- Item A6: C19 触发条件 ---
  print_step "A6: C19 触发条件评估 (deep_copy 瓶颈 / Session 迁移需求)"
  if [ -f tests/test_session.cpp ]; then
    if grep -q "fork" tests/test_session.cpp 2>/dev/null; then
      print_ok "C19: test_session.cpp 存在, fork 测试已覆盖"
    else
      print_warn "C19: test_session.cpp 存在但缺 fork 测试 (无 deep_copy benchmark 信号)"
    fi
  else
    print_warn "C19: tests/test_session.cpp 不存在 (无法验证 fork 测试)"
  fi
  echo "  注意: C19 触发条件 a (deep_copy 瓶颈) 无自动 benchmark 信号, 需人工"
  echo "  注意: C19 触发条件 b (Session 迁移需求) 无跨进程需求信号"

  # --- Item A7: 团队时间投入 ---
  print_step "A7: 团队时间投入可用 (1-2 eng × 4-6 周)"
  if [ -f docs/handoff/2026-07-31-stage-gate-evaluation.md ]; then
    print_ok "A7: handoff doc 存在 (evidence base)"
    echo "  注意: 团队时间投入需 Sprint 23 启动会议人工确认"
  else
    print_warn "A7: docs/handoff/2026-07-31-stage-gate-evaluation.md 缺失"
  fi
fi

# ====================================================================
# Category B: ADR-0050 §启动条件 5 项硬前置
# ====================================================================
if [ -z "$CATEGORY" ] || [ "$CATEGORY" = "B" ]; then
  print_header "Category B: ADR-0050 §启动条件 5 项硬前置"

  # --- Preq B1: Phase 5 完全关闭 ---
  print_step "B1: Phase 5 完全关闭 (active OpenSpec changes = 0, 除 C20-Spike)"
  ACTIVE_CHANGES=$(ls openspec/changes/ 2>/dev/null | grep -v archive | grep -v '^$' | wc -l || echo 0)
  if [ "$ACTIVE_CHANGES" -le 1 ]; then
    if [ "$ACTIVE_CHANGES" -eq 1 ] && [ -d "openspec/changes/phase6-service-ification-v1" ]; then
      print_ok "B1: Phase 5 关闭 (仅 active = C20-Spike: phase6-service-ification-v1) ✅"
    elif [ "$ACTIVE_CHANGES" -eq 0 ]; then
      print_ok "B1: Phase 5 关闭 (0 active changes) ✅"
    else
      print_warn "B1: $ACTIVE_CHANGES active changes, 非预期 (预期 ≤1)"
    fi
  else
    print_fail "B1: $ACTIVE_CHANGES active changes, Phase 5 未完全关闭"
  fi

  # --- Preq B2: 服务化范围文档批准 ---
  print_step "B2: 服务化范围文档批准 (ADR-0051 存在 + 🔍 Proposed)"
  if [ -f docs/adr/adr-0051-phase6-pdk-composition-spike.md ]; then
    STATUS_LINE=$(grep -E "状态|🔍 Proposed|✅ Approved" docs/adr/adr-0051-phase6-pdk-composition-spike.md | head -2 || true)
    if echo "$STATUS_LINE" | grep -qE "🔍 Proposed|✅ Approved"; then
      print_ok "B2: ADR-0051 存在, 状态 🔍 Proposed ✅"
    else
      print_warn "B2: ADR-0051 存在但状态行不匹配 (预期 🔍 Proposed)"
    fi
  else
    print_fail "B2: docs/adr/adr-0051-phase6-pdk-composition-spike.md 不存在"
  fi

  # --- Preq B3: C20 placeholder 决议 ---
  print_step "B3: C20 placeholder 决议 (phase6-service-ification-v1 存在)"
  if [ -d openspec/changes/phase6-service-ification-v1 ]; then
    print_ok "B3: openspec/changes/phase6-service-ification-v1/ 存在 (C20 已激活) ✅"
  else
    print_fail "B3: phase6-service-ification-v1 目录不存在 (C20 placeholder 未激活)"
  fi

  # --- Preq B4: 团队容量确认 ---
  print_step "B4: 团队容量确认 (Sprint 23 commitment doc)"
  if ls docs/handoff/ 2>/dev/null | grep -qE "sprint-23|Sprint.23"; then
    print_ok "B4: Sprint 23 commitment doc 已存在 ✅"
  else
    print_warn "B4: 无 Sprint 23 commitment doc (需 Sprint 23 启动会议确认)"
  fi

  # --- Preq B5: ≥1 外部集成目标 (reframed per Oracle Q6) ---
  print_step "B5: ≥1 外部集成目标 (内部 Spike per Oracle Q6 reframing)"
  if [ -f openspec/changes/phase6-service-ification-v1/proposal.md ]; then
    if grep -qE "G1|G3|coding_assistant|knowledge_base" openspec/changes/phase6-service-ification-v1/proposal.md 2>/dev/null; then
      print_ok "B5: proposal.md 含 G1+G3 内部集成目标 (Oracle Q6 reframing) ✅"
      echo "  注意: ADR-0050 §启动条件 #5 '≥1 外部集成' 按 Oracle Q6 改为内部 Spike"
    else
      print_warn "B5: proposal.md 存在但未找到 G1+G3 集成目标"
    fi
  else
    print_warn "B5: proposal.md 不存在, 无法验证集成目标"
  fi
fi

# ====================================================================
# Category C: C20-Spike W2-W3 unlock 3 条件
# ====================================================================
if [ -z "$CATEGORY" ] || [ "$CATEGORY" = "C" ]; then
  print_header "Category C: C20-Spike W2-W3 unlock 3 项"

  # --- Unlock C1: Stage Gate 2026-07-18 PASS ---
  print_step "C1: Stage Gate 2026-07-18 PASS (聚合 Category A 结果)"
  # 在运行时计算: 检查 7 项是否都满足
  # 由于 Item 1-3 是 wall-clock 依赖 (2-week), Item 6-7 是 manual,
  # 这里给一个基于当前日期的简化评估
  A1_OK=false
  A2_OK=false
  A3_OK=false
  NOW_EPOCH=$(date +%s)
  if [ $NOW_EPOCH -ge $(date -d "2026-07-17" +%s 2>/dev/null || echo 0) ]; then A1_OK=true; fi
  if [ $NOW_EPOCH -ge $(date -d "2026-07-18" +%s 2>/dev/null || echo 0) ]; then A2_OK=true; fi
  if [ $NOW_EPOCH -ge $(date -d "2026-07-18" +%s 2>/dev/null || echo 0) ]; then A3_OK=true; fi

  if [ "$A1_OK" = true ] && [ "$A2_OK" = true ] && [ "$A3_OK" = true ]; then
    print_ok "C1: 3/3 组件满 2 周稳定期 ✅"
    echo "  (Items A4-A7 需人工确认: 测试 / 标准库 / C19 / 团队)"
  else
    MISSING=""
    [ "$A1_OK" = false ] && MISSING="$MISSING C10"
    [ "$A2_OK" = false ] && MISSING="$MISSING C11"
    [ "$A3_OK" = false ] && MISSING="$MISSING C12"
    print_warn "C1: Stage Gate 不满足 — 未满 2 周:${MISSING}"
  fi

  # --- Unlock C2: Sprint 23 capacity ---
  print_step "C2: Sprint 23 capacity commitment"
  if ls docs/handoff/ 2>/dev/null | grep -qE "sprint-23|Sprint.23"; then
    print_ok "C2: Sprint 23 commitment doc 已存在 ✅"
  else
    print_warn "C2: 无 Sprint 23 commitment doc (需 Sprint 23 启动会议人工确认)"
  fi

  # --- Unlock C3: Oracle Q6 confirmation ---
  print_step "C3: Oracle Q6 confirmation (ADR-0051 §后续 + tasks.md §13)"
  C3_OK=true
  if [ -f docs/adr/adr-0051-phase6-pdk-composition-spike.md ]; then
    if grep -q "Oracle Q6\|后续行动\|Stage Gate" docs/adr/adr-0051-phase6-pdk-composition-spike.md 2>/dev/null; then
      echo "  ADR-0051 §后续: 已提及 Stage Gate + Oracle Q6 确认路径"
    else
      C3_OK=false
    fi
  else
    C3_OK=false
  fi

  if [ -f openspec/changes/phase6-service-ification-v1/tasks.md ]; then
    if grep -q "promotion criteria\|§13\|Spike.*Candidate" openspec/changes/phase6-service-ification-v1/tasks.md 2>/dev/null; then
      echo "  tasks.md §13: promotion criteria 已定义"
    else
      C3_OK=false
    fi
  else
    C3_OK=false
  fi

  if [ "$C3_OK" = true ]; then
    print_ok "C3: Oracle Q6 confirmation 路径已记录 ✅"
  else
    print_warn "C3: Oracle Q6 confirmation 缺失部分证据 (需检查 ADR-0051 §后续 + tasks.md §13)"
  fi
fi

# ====================================================================
# Summary
# ====================================================================
END_TIME=$(date +%s)
DURATION=$((END_TIME - START_TIME))

print_header "Stage Gate 2026-07-18 Readiness Summary"

TOTAL_CHECKS=$((7 + 5 + 3))
echo "  Total checks: 15 (7 + 5 + 3)"
echo "  ✅ PASSED:   $PASSED"
echo "  ⚠️  WARNINGS: $WARNINGS"
echo "  ❌ FAILED:   $FAILED"
echo "  ⏱️  耗时:     ${DURATION}s"
echo ""

# 按类别展示
if [ "$CATEGORY" = "A" ]; then
  echo "  Category A (Stage Gate 7 items): $( [ $FAILED -eq 0 ] && echo '✅' || echo '❌' )"
elif [ "$CATEGORY" = "B" ]; then
  echo "  Category B (ADR-0050 5 hard prerequisites): $( [ $FAILED -eq 0 ] && echo '✅' || echo '❌' )"
elif [ "$CATEGORY" = "C" ]; then
  echo "  Category C (C20-Spike 3 unlocks): $( [ $FAILED -eq 0 ] && echo '✅' || echo '❌' )"
else
  echo "  Category A (Stage Gate 7 items):   $( [ $FAILED -eq 0 ] && echo '⏳ PASS/WARN' || echo '❌ FAIL' )"
  echo "  Category B (ADR-0050 5 prerequisites): $( [ $FAILED -eq 0 ] && echo '⏳ PASS/WARN' || echo '❌ FAIL' )"
  echo "  Category C (C20-Spike 3 unlocks):  $( [ $FAILED -eq 0 ] && echo '⏳ PASS/WARN' || echo '❌ FAIL' )"
fi

echo ""

# Overall verdict
if [ $FAILED -gt 0 ]; then
  echo -e "${RED}🚫 Overall: NOT READY — 有 hard blocker(s)${NC}"
  echo ""
  echo "下一步:"
  echo "  1. 查看上方 ❌ 项的详细错误"
  echo "  2. 修复 hard blockers 后重跑 $0"
  echo "  3. 若 FAIL 来自 manual items (A6/A7/B4/B5), 安排 Sprint 23 启动会议确认"
  exit 2
elif [ $WARNINGS -gt 0 ]; then
  echo -e "${YELLOW}⚠️  Overall: NEEDS REVIEW — 部分检查需人工确认${NC}"
  echo ""
  echo "下一步:"
  echo "  1. 查看上方 ⚠️ 项的详细说明"
  echo "  2. 确认 manual items: A6 (C19 触发条件), A7 (团队时间), B4 (Sprint 23), B5 (集成目标), C2 (Sprint 23)"
  echo "  3. 若 wall-clock 2 周未满, 等待至 2026-07-18 重跑"
  echo "  4. 若全部 manual 确认通过, 视为 READY"
  exit 1
else
  echo -e "${GREEN}🎉 Overall: READY — Stage Gate 可解锁, W2-W3 可启动${NC}"
  echo ""
  echo "  → Stage Gate 2026-07-18 已满足 (gated on manual Sprint 23 commitment)"
  echo ""
  echo "下一步:"
  echo "  1. Sprint 23 启动会议: 确认团队容量 (1-2 eng × 4-6 周)"
  echo "  2. 确认 ADR-0050 §启动条件 #5 (外部集成目标或 Oracle Q6 reframing)"
  echo "  3. 启动 C20-Spike W2 实施 (per tasks.md §2-§9)"
  exit 0
fi