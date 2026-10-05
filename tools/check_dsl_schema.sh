#!/usr/bin/env bash
# tools/check_dsl_schema.sh
# CI script: verify lib/*.md .agent.md files use 'arguments:' canonical key
# (not 'args:') per docs/specs/dsl.md §5.2.
#
# Per openspec/changes/2026-09-30-fix-lib-loop-args-parsing/D4 SHIP-with-fixes
# (grep-based, since yaml.safe_load chokes on Markdown frontmatter).
#
# Usage: bash tools/check_dsl_schema.sh
# Exit: 0 = pass, 1 = drift detected (with file:context)

set -euo pipefail

violations=0
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"
lib_dir="$repo_root/lib"

if [ ! -d "$lib_dir" ]; then
  echo "ERROR: lib/ directory not found at $lib_dir"
  exit 2
fi

for md in $(find "$lib_dir" -name "*.agent.md"); do
  # Extract YAML fence contents between ```yaml and ``` markers.
  # awk command reads from `md` and prints lines inside ```yaml ... ``` blocks.
  yaml=$(awk '
    /^```yaml$/ { flag = 1; next }
    /^```$/     { flag = 0 }
    flag { print }
  ' "$md")

  # Skip files with no YAML fence.
  [ -z "$yaml" ] && continue

  # Check if YAML contains tool_call nodes with `args:` instead of `arguments:`.
  # Pattern: a line starting with whitespace + 'args:' at end of line.
  if echo "$yaml" | grep -nE '^\s*args:\s*$' >/dev/null; then
    echo "ERROR: $md: tool_call nodes use 'args:' (must use 'arguments:' per docs/specs/dsl.md §5.2)"
    violations=$((violations + 1))
  fi
done

if [ $violations -gt 0 ]; then
  echo ""
  echo "DSL schema drift detected: $violations file(s) using deprecated 'args:' key"
  echo "Fix: rename 'args:' to 'arguments:' in lib/*.agent.md (per docs/specs/dsl.md §5.2)"
  exit 1
fi

echo "DSL schema check: PASS (all lib/*.agent.md use 'arguments:' canonical key)"