#!/usr/bin/env python3
"""measure_prompt_baseline — T21 ADR-0074 D3 baseline 测量 (Mock, 零外部 API)。

用 3 个 persona (gpt-4 / claude / deepseek) 的确定性 MockLLM 在 held-out
golden tasks 上测量 2 个指标 (parse-valid / task-success)，输出 baseline.json。

V1 边界: MockLLM 模拟，不触发真实 LLM API 调用；确定性 (无随机)。

用法:
  python3 tools/baseline/measure_prompt_baseline.py \
      --golden-dir lib/prompt/golden --output baseline.json
"""
import argparse
import json
import pathlib
import sys
import time
from typing import Final

PERSONAS: Final[dict[str, dict[str, float]]] = {
    "gpt-4": {"L1": 0.95, "L2": 0.90, "L3": 0.82, "success_factor": 0.95},
    "claude": {"L1": 0.92, "L2": 0.86, "L3": 0.78, "success_factor": 0.90},
    "deepseek": {"L1": 0.88, "L2": 0.80, "L3": 0.70, "success_factor": 0.85},
}


def load_golden_tasks(golden_dir: pathlib.Path) -> list[dict[str, object]]:
    tasks: list[dict[str, object]] = []
    if not golden_dir.is_dir():
        print(f"ERROR: golden dir not found: {golden_dir}", file=sys.stderr)
        raise SystemExit(1)
    for path in sorted(golden_dir.glob("*.json")):
        tasks.append(json.loads(path.read_text(encoding="utf-8")))
    return tasks


def measure_persona(persona: dict[str, float],
                    tasks: list[dict[str, object]]) -> dict[str, float]:
    parse_rates: list[float] = []
    success_rates: list[float] = []
    for task in tasks:
        difficulty = str(task.get("difficulty", "L1"))
        parse_rate = persona[difficulty]
        parse_rates.append(parse_rate)
        success_rates.append(parse_rate * persona["success_factor"])
    return {
        "parse_valid": sum(parse_rates) / len(parse_rates) if parse_rates else 0.0,
        "task_success": sum(success_rates) / len(success_rates) if success_rates else 0.0,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="T21 prompt baseline (Mock)")
    parser.add_argument("--golden-dir", type=pathlib.Path, default="lib/prompt/golden")
    parser.add_argument("--output", type=pathlib.Path, default="baseline.json")
    args = parser.parse_args()

    tasks = load_golden_tasks(args.golden_dir)
    llms = {
        name: measure_persona(cfg, tasks)
        for name, cfg in PERSONAS.items()
    }
    baseline = {
        "baseline_id": f"{time.strftime('%Y-%m-%d')}-t21-prompt-baseline",
        "golden_tasks": len(tasks),
        "mock_mode": True,
        "llms": llms,
        "generated_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
    }
    args.output.write_text(
        json.dumps(baseline, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    print(f"Baseline written to: {args.output} ({len(tasks)} tasks)")
    return 0


if __name__ == "__main__":
    sys.exit(main())