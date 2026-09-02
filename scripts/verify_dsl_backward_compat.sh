#!/bin/bash
# scripts/verify_dsl_backward_compat.sh
# C6+C7 向后兼容性验证: 委托 ctest test_dsl_extensions::"Backward compat" 用例
# (直接遍历 examples/**/*.agent.md + lib/**/*.agent.md 通过 MarkdownParser 解析验证)
# 依据: openspec/changes/from-roadmap-phase-6c-execution-dsl/specs/dsl-extensions/spec.md

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_ROOT}/build"

if [[ ! -d "$BUILD_DIR" ]]; then
    echo "ERROR: Build directory not found at $BUILD_DIR"
    echo "Please build first: mkdir -p build && cd build && cmake .. && make -j\$(nproc)"
    exit 1
fi

echo "=== DSL Backward Compatibility Verification ==="
echo "Project root: $PROJECT_ROOT"
echo ""

# 统计待测文件
agent_file_count=$(find "$PROJECT_ROOT/examples" "$PROJECT_ROOT/lib" -name "*.agent.md" 2>/dev/null | wc -l)
echo "Found $agent_file_count .agent.md files in examples/ + lib/"
echo ""

# 委托 ctest test_dsl_extensions 的 Backward compat 用例 (单测已覆盖完整遍历逻辑)
echo "Running ctest -R \"test_dsl_extensions\" ..."
if (cd "$BUILD_DIR" && ctest -R "^test_dsl_extensions$" --output-on-failure); then
    echo ""
    echo "SUCCESS: All .agent.md files parse correctly. Backward compatibility maintained."
    exit 0
else
    echo ""
    echo "ERROR: Backward compat test FAILED. See ctest output above for details."
    exit 1
fi