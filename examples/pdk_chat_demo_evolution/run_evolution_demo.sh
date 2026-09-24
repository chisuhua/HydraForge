#!/usr/bin/env bash
# examples/pdk_chat_demo_evolution/run_evolution_demo.sh
# L2 reference example demo dispatcher (mock + real-LLM modes)
#
# Per `openspec/changes/pdk-chat-demo-evolution-reference-example/`:
# bash wrapper for end-to-end demo.  Passes `--context-file` through to
# the binary, plus provider / capture-mode flags.
#
# Usage:
#   ./run_evolution_demo.sh <mock|real-llm> [context-file.jsonl]
#
# Defaults:
#   mock mode -> --provider mock (zero API key required)
#   context-file -> examples/pdk_chat_demo_evolution/fixtures/contexts/code-class-context.jsonl
#
# Examples:
#   ./run_evolution_demo.sh mock
#   ./run_evolution_demo.sh mock my-fixtures/3-class.jsonl --trace-events
#   DEEPSEEK_API_KEY=sk-... ./run_evolution_demo.sh real-llm
#   ./run_evolution_demo.sh mock --release-metrics --ablation-mode=full

set -euo pipefail

BINARY="${BINARY:-build/examples/pdk_chat_demo_evolution/pdk_chat_demo_evolution}"
MODE="${1:-mock}"
shift || true

# Default context file: SHIPPED code-class fixture (Batch 1)
DEFAULT_CTX="examples/pdk_chat_demo_evolution/fixtures/contexts/code-class-context.jsonl"

# Separate context file positional arg from extra flags
CTX="$DEFAULT_CTX"
EXTRA_ARGS=()
while [ $# -gt 0 ]; do
    case "$1" in
        *.jsonl)
            CTX="$1"
            shift
            ;;
        --*)
            EXTRA_ARGS+=("$1")
            shift
            ;;
        *)
            EXTRA_ARGS+=("$1")
            shift
            ;;
    esac
done

if [ ! -x "$BINARY" ]; then
    echo "ERROR: $BINARY not built.  Run: cmake --build build --target pdk_chat_demo_evolution" >&2
    exit 1
fi

case "$MODE" in
    mock)
        "$BINARY" --mock --context-file "$CTX" "${EXTRA_ARGS[@]}"
        ;;
    real-llm)
        if [ -z "${DEEPSEEK_API_KEY:-}" ]; then
            echo "ERROR: DEEPSEEK_API_KEY required for real-llm mode" >&2
            exit 2
        fi
        "$BINARY" --provider real_llm_deepseek --context-file "$CTX" "${EXTRA_ARGS[@]}"
        ;;
    *)
        echo "Usage: $0 <mock|real-llm> [context-file.jsonl] [extra-flags...]" >&2
        exit 3
        ;;
esac