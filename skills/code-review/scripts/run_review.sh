#!/bin/bash
# scripts/run_review.sh
# Code Review Skill 的辅助脚本（在 SkillInterpreter 中被调用）
# 关联: skills/code-review/SKILL.md

set -e

CODE="$1"
LANGUAGE="$2"
SEVERITY="${3:-medium}"

if [ -z "$CODE" ] || [ -z "$LANGUAGE" ]; then
    echo "Usage: $0 <code> <language> [severity]"
    exit 1
fi

# 模拟审查（实际实现会调用 LLM）
echo "Reviewing ${LANGUAGE} code with ${SEVERITY} severity..."

# 返回 JSON 格式的审查结果（示例）
cat <<EOF
{
  "issues": [],
  "summary": "No issues found (mock implementation)",
  "total_issues": 0
}
EOF