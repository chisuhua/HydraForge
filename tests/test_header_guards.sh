#!/bin/bash
# T-007 Header Guard Naming Convention Test
# TDD Phase 1: 先写测试，验证当前不符合规范

set -e

ERRORS=0
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# 定义期望的规范: AGENTICDSL_<MODULE>_<FILE>_H
# 例如:
#   src/core/engine.h -> AGENTICDSL_CORE_ENGINE_H
#   src/common/tools/registry.h -> AGENTICDSL_COMMON_TOOLS_REGISTRY_H

# 需要修复的文件及其期望的宏名
declare -A EXPECTED_GUARDS=(
    ["src/common/tools/registry.h"]="AGENTICDSL_COMMON_TOOLS_REGISTRY_H"
    ["src/core/types/node.h"]="AGENTICDSL_CORE_TYPES_NODE_H"
    ["src/core/types/context.h"]="AGENTICDSL_CORE_TYPES_CONTEXT_H"
    ["src/core/types/common.h"]="AGENTICDSL_CORE_TYPES_COMMON_H"
    ["src/modules/scheduler/resource_manager.h"]="AGENTICDSL_MODULES_SCHEDULER_RESOURCE_MANAGER_H"
)

echo "=== T-007 Header Guard Test ==="
echo "检查 header guards 是否遵循 AGENTICDSL_<MODULE>_<FILE>_H 规范"
echo ""

for file in "${!EXPECTED_GUARDS[@]}"; do
    expected="${EXPECTED_GUARDS[$file]}"
    filepath="$ROOT_DIR/$file"

    if [ ! -f "$filepath" ]; then
        echo "⚠️  文件不存在: $file (跳过)"
        continue
    fi

    # 提取 #ifndef 后的宏名
    actual=$(grep -m1 "^#ifndef" "$filepath" | awk '{print $2}')

    if [ "$actual" = "$expected" ]; then
        echo "✅ $file: $actual"
    else
        echo "❌ $file: $actual (期望: $expected)"
        ERRORS=$((ERRORS + 1))
    fi
done

echo ""
echo "=== 测试结果 ==="
if [ $ERRORS -eq 0 ]; then
    echo "✅ 所有 header guards 符合规范!"
    exit 0
else
    echo "❌ 发现 $ERRORS 个不符合规范的 header guards"
    exit 1
fi
