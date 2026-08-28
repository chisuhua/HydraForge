#!/usr/bin/env python3
"""export_training_data — T21 ADR-0074 D6 JSONL 训练数据导出。

读取 golden tasks 并导出 JSONL: {"prompt", "response", "reward", "metadata"}。
reward = validation_rules 满足比例 (golden 为参考正确解, 通常 = 1.0)。

用法:
  python3 tools/prompt/export_training_data.py \
      --golden-dir lib/prompt/golden --output train.jsonl
"""
import argparse
import json
import pathlib
import sys


def rule_body(rule: str) -> str:
    if len(rule) > 2 and rule[1] == ":" and rule[0] in ("P", "S"):
        return rule[2:]
    return rule


def reward_for(task: dict[str, object]) -> float:
    rules_raw = task.get("validation_rules")
    rules: list[object] = rules_raw if isinstance(rules_raw, list) else []
    if not rules:
        return 0.0
    expected = str(task.get("expected_output", ""))
    satisfied = sum(1 for r in rules if rule_body(str(r)) in expected)
    return satisfied / len(rules)


def export(golden_dir: pathlib.Path, output: pathlib.Path) -> int:
    tasks: list[dict[str, object]] = []
    if not golden_dir.is_dir():
        print(f"ERROR: golden dir not found: {golden_dir}", file=sys.stderr)
        return 1
    for path in sorted(golden_dir.glob("*.json")):
        tasks.append(json.loads(path.read_text(encoding="utf-8")))

    lines: list[str] = []
    for task in tasks:
        record = {
            "prompt": task.get("input", ""),
            "response": task.get("expected_output", ""),
            "reward": reward_for(task),
            "metadata": {
                "task_id": task.get("task_id", ""),
                "domain": task.get("domain", ""),
                "difficulty": task.get("difficulty", ""),
            },
        }
        lines.append(json.dumps(record, ensure_ascii=False))
    output.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Exported {len(lines)} records to {output}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="T21 JSONL training data export")
    parser.add_argument("--golden-dir", type=pathlib.Path, default="lib/prompt/golden")
    parser.add_argument("--output", type=pathlib.Path, default="train.jsonl")
    args = parser.parse_args()
    return export(args.golden_dir, args.output)


if __name__ == "__main__":
    sys.exit(main())