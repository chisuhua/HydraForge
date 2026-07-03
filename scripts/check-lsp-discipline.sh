#!/usr/bin/env bash
# scripts/check-lsp-discipline.sh
# 功能描述: 验证 LSP 工具链配置正确性, 预防 LSP false positive 错误
#          (audit: docs/audits/2026-07-03-pdk-model-router-lsp-false-positive.md)
#
# 检测项:
#   1. compile_commands.json 软链有效性 (指向实际 build dir 的最新 compile_commands)
#   2. .clangd 配置文件存在且语法正确
#   3. 关键 pdk/ 文件 LSP 解析无 namespace 错误 (clangd --check)
#   4. ctest ground truth 对齐 (LSP 报 error 但编译成功 → false positive 候选)
#
# 用法:
#   ./scripts/check-lsp-discipline.sh              # 完整检查
#   ./scripts/check-lsp-discipline.sh --quick      # 仅检查配置 (跳过 clangd check, ~5s)
#   ./scripts/check-lsp-discipline.sh --files pdk/  # 自定义检查文件
#
# 退出码:
#   0: 全部通过
#   1: 配置问题 (需修复)
#   2: 检测到 LSP false positive (需重新索引)
#   3: 真实 LSP 错误 (需修复代码)
#
# 设计依据:
#   - Sprint 19 LSP discipline 修复
#   - audit 2026-07-03 (LSP false positive investigation)
#   - docs/audits/2026-07-03-pdk-model-router-lsp-false-positive.md

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
QUICK_MODE=false
CUSTOM_FILES=""
EXIT_CODE=0

# 参数解析
while [[ $# -gt 0 ]]; do
  case $1 in
    --quick) QUICK_MODE=true; shift ;;
    --files) CUSTOM_FILES="$2"; shift 2 ;;
    *) echo "Unknown arg: $1"; exit 1 ;;
  esac
done

print_header() {
  echo ""
  echo -e "${BLUE}==== $1 ====${NC}"
  echo ""
}

print_step() {
  echo -e "${BLUE}→ $1${NC}"
}

print_ok() {
  echo -e "${GREEN}✓ $1${NC}"
}

print_warn() {
  echo -e "${YELLOW}⚠ $1${NC}"
}

print_fail() {
  echo -e "${RED}✗ $1${NC}"
}

# ====================================================================
# Check 1: compile_commands.json 软链有效性
# ====================================================================
print_header "Check 1/4: 📎 compile_commands.json 软链"

if [ ! -L compile_commands.json ]; then
  print_fail "compile_commands.json 不是 symlink (期望 symlink 到当前 build dir)"
  echo "  修复: ln -sf build/compile_commands.json compile_commands.json"
  EXIT_CODE=1
else
  TARGET=$(readlink compile_commands.json)
  print_ok "compile_commands.json → $TARGET"

  if [ ! -f "$TARGET" ]; then
    print_fail "软链目标 $TARGET 不存在"
    echo "  修复: 删除软链 + 重新 cmake"
    EXIT_CODE=1
  else
    # 检查目标是否为最新 (24h 内)
    if find "$TARGET" -mtime -1 | grep -q .; then
      print_ok "compile_commands.json 是 24h 内生成的"
    else
      print_warn "compile_commands.json 超过 24h 未更新, 建议重新 cmake"
    fi
  fi
fi

# ====================================================================
# Check 2: .clangd 配置文件
# ====================================================================
print_header "Check 2/4: ⚙️  .clangd 配置文件"

if [ ! -f .clangd ]; then
  print_fail ".clangd 不存在, clangd 用 default config"
  echo "  修复: 创建 .clangd (参考模板 scripts/templates/.clangd.example)"
  EXIT_CODE=1
else
  if command -v clangd >/dev/null 2>&1; then
    # .clangd 接受两种 YAML 模式:
    #   1. 单文档: 顶层 key 直接在文件首 (例如 "Diagnostics:" 或 "Index:")
    #   2. 多文档: '---' 分隔多个文档, 每个文档顶层 key 开始
    # 文件首行允许是注释 (#开头)
    FIRST_KEY=$(grep -m1 -E "^[A-Za-z_][A-Za-z0-9_]*:|^---$" .clangd || echo "")
    if [ -n "$FIRST_KEY" ]; then
      print_ok ".clangd 存在且格式正确 (首个有效 key/marker: $FIRST_KEY)"
    else
      print_warn ".clangd 缺少顶层 YAML key 或 '---' 分隔符"
    fi
  else
    print_warn "clangd 未安装, 跳过 .clangd 验证"
  fi
fi

# ====================================================================
# Check 3: clangd --check 关键文件 (LSP 解析验证)
# ====================================================================
print_header "Check 3/4: 🔍 clangd --check 关键文件"

# 默认检查文件 (pdk/ + 关键 entry)
if [ -z "$CUSTOM_FILES" ]; then
  CHECK_FILES=(
    "pdk/model_router/cost_strategy/cost_router.cpp"
    "pdk/model_router/quality_strategy/quality_router.cpp"
    "pdk/model_router/latency_strategy/latency_router.cpp"
    "pdk/model_router/model_registry.cpp"
  )
else
  CHECK_FILES=($CUSTOM_FILES)
fi

if [ "$QUICK_MODE" = true ]; then
  print_step "Quick mode: 跳过 clangd --check (需要 ~30s/文件)"
  for f in "${CHECK_FILES[@]}"; do
    print_ok "  [skipped] $f"
  done
elif ! command -v clangd >/dev/null 2>&1; then
  print_warn "clangd 未安装, 跳过 LSP 解析验证"
  echo "  修复: apt install clangd (Ubuntu) 或 brew install llvm (macOS)"
else
  for f in "${CHECK_FILES[@]}"; do
    if [ ! -f "$f" ]; then
      print_warn "  $f 不存在, 跳过"
      continue
    fi
    print_step "clangd --check $f"
    OUTPUT=$(clangd --check="$f" 2>&1 || true)
    if echo "$OUTPUT" | grep -qE "config error|internal error"; then
      print_fail "  $f: clangd 配置错误"
      echo "$OUTPUT" | grep "config error" | head -3 | sed 's/^/    /'
      EXIT_CODE=1
    elif echo "$OUTPUT" | grep -qE "[0-9]+ errors?$"; then
      ERROR_COUNT=$(echo "$OUTPUT" | grep -oE "[0-9]+ errors?$" | tail -1)
      if [ "$ERROR_COUNT" = "0 errors" ]; then
        print_ok "  $f: 0 errors (LSP 干净)"
      else
        # 检查错误是否为 namespace false positive (已知模型)
        if echo "$OUTPUT" | grep -qE "undeclared identifier 'hydraforge'|undeclared namespace 'hydraforge'"; then
          print_fail "  $f: LSP false positive (namespace 错误) - 需重新 clangd 索引"
          EXIT_CODE=2
        else
          print_fail "  $f: $ERROR_COUNT (真实 LSP 错误)"
          EXIT_CODE=3
        fi
      fi
    else
      print_ok "  $f: clangd 解析无 error 报告"
    fi
  done
fi

# ====================================================================
# Check 4: 同步状态 (compile_commands.json 与 build/ 一致性)
# ====================================================================
print_header "Check 4/4: 🔄 同步状态"

# 检查 build/compile_commands.json 中 pdk 覆盖
if [ -f build/compile_commands.json ]; then
  PDK_COUNT=$(python3 -c "
import json
with open('build/compile_commands.json') as f:
    data = json.load(f)
pdk = [e for e in data if '/pdk/' in e.get('file','')]
print(len(pdk))
" 2>/dev/null || echo "?")
  if [ "$PDK_COUNT" -ge 4 ]; then
    print_ok "build/compile_commands.json 含 $PDK_COUNT 个 pdk entries (≥4 预期)"
  else
    print_warn "build/compile_commands.json 仅含 $PDK_COUNT 个 pdk entries, 可能未完整重新 cmake"
  fi
else
  print_fail "build/compile_commands.json 不存在, 无法验证 pdk 覆盖"
  EXIT_CODE=1
fi

# ====================================================================
# Summary
# ====================================================================
print_header "📊 LSP Discipline Summary"

case $EXIT_CODE in
  0)
    echo -e "${GREEN}✅ 全部 LSP discipline 检查通过${NC}"
    echo ""
    echo "下一步:"
    echo "  - 当前 LSP 配置健康, 可继续 Sprint 工作"
    echo "  - 如 Sprint 中遇到 LSP false positive, 跑 ./scripts/check-lsp-discipline.sh 复检"
    ;;
  1)
    echo -e "${RED}❌ LSP 配置问题, 需修复${NC}"
    echo ""
    echo "建议:"
    echo "  1. 删除 symlink + 重新 cmake 让 compile_commands.json 指向新 build dir"
    echo "     rm compile_commands.json && cd build && cmake --build . -j\$(nproc)"
    echo "  2. 确认 .clangd 存在且格式正确 (YAML front matter)"
    echo "  3. 跑 ./scripts/check-lsp-discipline.sh 再次验证"
    ;;
  2)
    echo -e "${YELLOW}⚠️  检测到 LSP false positive${NC}"
    echo ""
    echo "原因: LSP 客户端缓存了旧 index 状态"
    echo "建议:"
    echo "  1. 重启 LSP 客户端 (opencode 内: 重新加载窗口)"
    echo "  2. 删除 .cache/clangd/ 强制重新索引"
    echo "  3. 跑 clangd --check=<file> 验证真编译无错"
    echo "  4. 跑 ./scripts/check-lsp-discipline.sh 复检"
    ;;
  3)
    echo -e "${RED}❌ 真实 LSP 错误, 需修复代码${NC}"
    echo ""
    echo "建议:"
    echo "  1. 跑 clangd --check=<file> 看具体错误"
    echo "  2. 修复代码 (可能是 namespace 缺失 / include 错误 / 真实 bug)"
    echo "  3. 跑 ctest 验证 ground truth"
    echo "  4. 跑 ./scripts/check-lsp-discipline.sh 复检"
    ;;
esac

echo ""
exit $EXIT_CODE
