#!/usr/bin/env bash
# scripts/sprint-closeout.sh
# 功能描述: Sprint 收官 wrapper — 自动运行 ship gate + Drift Detection
#          自 2026-06-26 起，每个 Sprint 收官前必须执行此脚本
# 设计依据: docs/superpowers/plans/2026-06-26-sprint-11-to-18-roadmap.md §9 Review Gates
#          docs/roadmap-status.md §六 Sprint 结束前检查
# 用法:
#   ./scripts/sprint-closeout.sh              # 完整 sprint-closeout
#   ./scripts/sprint-closeout.sh --drift-only # 仅 Drift Detection (快速检查)
#   ./scripts/sprint-closeout.sh --no-ctest   # 跳过 ctest (CI 失败调试时)

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
RUN_ASAN_TSAN=false
RUN_DRIFT=true
RUN_LINT=true
RUN_DOCS_AUDIT=true
RUN_OPENSPEC_VALIDATE=true
DRIFT_ONLY=false

# 解析参数
while [[ $# -gt 0 ]]; do
  case "$1" in
    --drift-only)
      DRIFT_ONLY=true
      shift
      ;;
    --no-ctest)
      RUN_CTEST=false
      shift
      ;;
    --with-asan-tsan)
      RUN_ASAN_TSAN=true
      shift
      ;;
    --help|-h)
      echo "用法: $0 [选项]"
      echo ""
      echo "选项:"
      echo "  --drift-only       仅运行 Drift Detection"
      echo "  --no-ctest         跳过 ctest"
      echo "  --with-asan-tsan   同时运行 ASan/TSan (耗时较长)"
      echo "  --help, -h         显示帮助"
      exit 0
      ;;
    *)
      echo -e "${RED}未知选项: $1${NC}"
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

# 标题
print_header "Sprint Closeout — $(date '+%Y-%m-%d %H:%M:%S')"
echo "Repo: $REPO_ROOT"
echo ""

# ====================================================================
# Step 1: Drift Detection (Roadmap-Driven Development 强制)
# ====================================================================
if [ "$RUN_DRIFT" = true ]; then
  print_header "Step 1/6: 🔍 Drift Detection (Master Plan §9 Review Gates)"

  if [ ! -f tools/check_roadmap_drift.py ]; then
    print_fail "tools/check_roadmap_drift.py 不存在"
    exit 1
  fi

  set +e
  DRIFT_OUTPUT=$(python3 tools/check_roadmap_drift.py 2>&1)
  DRIFT_EXIT=$?
  set -e

  echo "$DRIFT_OUTPUT" | head -80

  if [ $DRIFT_EXIT -eq 0 ]; then
    print_ok "Drift Detection: 无 drift"
  elif [ $DRIFT_EXIT -eq 1 ]; then
    print_fail "Drift Detection: 有 CRITICAL drift, 必须先创建 fix change 再 Sprintf Closeout"
  fi

  if [ "$DRIFT_ONLY" = true ]; then
    print_header "Summary (--drift-only)"
    echo "  ✅ PASSED: $PASSED"
    echo "  ⚠️  WARNINGS: $WARNINGS"
    echo "  ❌ FAILED: $FAILED"
    exit $DRIFT_EXIT
  fi
fi

# ====================================================================
# Step 2: ctest (测试)
# ====================================================================
if [ "$RUN_CTEST" = true ]; then
  print_header "Step 2/6: 🧪 ctest (测试套件)"

  if [ ! -d build ]; then
    print_step "build/ 不存在, 跳过 ctest (CI 环境会跑)"
  else
    print_step "运行 ctest --output-on-failure..."
    if cd build && ctest --output-on-failure 2>&1 | tail -30; then
      cd ..
      print_ok "ctest 全绿"
    else
      cd ..
      print_fail "ctest 失败"
    fi
  fi

  if [ "$RUN_ASAN_TSAN" = true ]; then
    print_step "运行 ASan (--preset asan)..."
    if cmake --preset asan -DAGENTICDSL_BUILD_TESTS=ON > /tmp/asan.log 2>&1; then
      cd build-asan && ctest --output-on-failure > /tmp/asan-ctest.log 2>&1 && cd ..
      print_ok "ASan: 0 errors"
    else
      cd ..
      print_fail "ASan 构建失败, 见 /tmp/asan.log"
    fi

    print_step "运行 TSan (--preset tsan)..."
    if cmake --preset tsan -DAGENTICDSL_BUILD_TESTS=ON > /tmp/tsan.log 2>&1; then
      cd build-tsan && ctest --output-on-failure > /tmp/tsan-ctest.log 2>&1 && cd ..
      print_ok "TSan: 0 errors"
    else
      cd ..
      print_fail "TSan 构建失败, 见 /tmp/tsan.log"
    fi
  fi
fi

# ====================================================================
# Step 3: ADR lint
# ====================================================================
if [ "$RUN_LINT" = true ]; then
  print_header "Step 3/6: 📋 ADR lint"

  print_step "python3 tools/adr_lint.py..."
  if python3 tools/adr_lint.py 2>&1 | tail -10; then
    print_ok "ADR lint 通过"
  else
    print_fail "ADR lint 失败"
  fi
fi

# ====================================================================
# Step 4: Docs drift audit
# ====================================================================
if [ "$RUN_DOCS_AUDIT" = true ]; then
  print_header "Step 4/6: 📚 Docs drift audit"

  print_step "python3 tools/docs_drift_audit.py..."
  if python3 tools/docs_drift_audit.py 2>&1 | tail -20; then
    print_ok "Docs drift audit: 0 critical drift"
  else
    print_warn "Docs drift audit 有发现, 需人工 review"
  fi
fi

# ====================================================================
# Step 5: OpenSpec validate
# ====================================================================
if [ "$RUN_OPENSPEC_VALIDATE" = true ]; then
  print_header "Step 5/7: 📋 OpenSpec validate (所有 active changes)"

  OPENSPEC_CHANGES=$(openspec list 2>/dev/null | grep -oE '[0-9]{4}-[0-9]{2}-[0-9]{2}-[a-z0-9-]+' | sort -u || true)

  if [ -z "$OPENSPEC_CHANGES" ]; then
    print_step "无 active changes, 跳过"
  else
    VALIDATE_PASS=0
    VALIDATE_FAIL=0
    for change in $OPENSPEC_CHANGES; do
      if openspec validate "$change" >/dev/null 2>&1; then
        VALIDATE_PASS=$((VALIDATE_PASS + 1))
      else
        VALIDATE_FAIL=$((VALIDATE_FAIL + 1))
        print_fail "OpenSpec validate 失败: $change"
      fi
    done

    if [ $VALIDATE_FAIL -eq 0 ]; then
      print_ok "OpenSpec validate: $VALIDATE_PASS/$((VALIDATE_PASS + VALIDATE_FAIL)) changes 通过"
    fi
  fi
fi

# ====================================================================
# Step 6: LSP discipline (预防 LSP false positive)
# 设计依据: docs/audits/2026-07-03-pdk-model-router-lsp-false-positive.md
# ====================================================================
if [ -f scripts/check-lsp-discipline.sh ]; then
  print_header "Step 6/7: 🔍 LSP discipline (clangd 配置 + 解析验证)"

  print_step "scripts/check-lsp-discipline.sh --quick..."
  if ./scripts/check-lsp-discipline.sh --quick 2>&1 | tail -25; then
    print_ok "LSP discipline 通过"
  else
    LSP_EXIT=$?
    if [ $LSP_EXIT -eq 2 ]; then
      print_warn "LSP false positive 检测到, 需重启 LSP 客户端 (退出码 2)"
    elif [ $LSP_EXIT -eq 3 ]; then
      print_fail "真实 LSP 错误, 需修复代码 (退出码 3)"
      FAILED=$((FAILED + 1))
    else
      print_warn "LSP discipline 配置问题, 退出码 $LSP_EXIT"
    fi
  fi
else
  print_warn "scripts/check-lsp-discipline.sh 不存在, 跳过"
fi

# ====================================================================
# Step 7: 总结 + 下一步建议
# ====================================================================
END_TIME=$(date +%s)
DURATION=$((END_TIME - START_TIME))

print_header "Step 7/7: 📊 Summary"

echo "  ✅ PASSED:   $PASSED"
echo "  ⚠️  WARNINGS: $WARNINGS"
echo "  ❌ FAILED:   $FAILED"
echo "  ⏱️  耗时:     ${DURATION}s"
echo ""

# 退出码
if [ $FAILED -eq 0 ]; then
  echo -e "${GREEN}🎉 Sprint Closeout 通过!${NC}"
  echo ""
  echo "下一步:"
  echo "  1. 跑 git status 确认 clean"
  echo "  2. 更新 docs/roadmap-status.md §四 实施日志"
  echo "  3. 跑 openspec archive <change> 归档已 ship changes"
  echo "  4. 更新 Master plan §十/§十一/§十二 (如有 drift/调整/转向)"
  echo "  5. 更新 AGENTS.md § Recent Changes 追加 Sprint 收官记录"
  echo ""
  exit 0
else
  echo -e "${RED}🚨 Sprint Closeout 有失败项, 必须先修复.${NC}"
  echo ""
  echo "下一步:"
  echo "  1. 查看上方 ❌ 项的详细错误"
  echo "  2. 修复后再跑 ./scripts/sprint-closeout.sh"
  echo "  3. 如属预期偏差 (e.g. pre-existing sanitizer), 创建独立 tracking change"
  echo ""
  exit 1
fi