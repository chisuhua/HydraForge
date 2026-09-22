#!/usr/bin/env python3
"""Wave 3 Phase 1 D3 训练数据准备脚本 (ADR-0078 D3 + ADR-0074 D6).

消费 ADR-0074 D6 baseline JSONL, 每行加 source 字段 (baseline/failure/agenticmind),
过滤 parse_valid && task_success (缺失字段默认 true — ADR-0074 V1 实际 schema 兼容),
输出到 data/wave-3-training-data.jsonl.

设计依据: openspec/changes/wave-3-finetune-base-model-pilot-phase1/
          design.md D3-1/D3-2/D3-3 + specs/wave-3-finetune-base-model/spec.md R4.
"""

import argparse
import json
import sys
from pathlib import Path

VALID_SOURCES = {"baseline", "failure", "agenticmind"}


def parse_args(argv):
    parser = argparse.ArgumentParser(
        description="ADR-0074 D6 JSONL → Wave 3 训练数据 (加 source + 过滤)"
    )
    parser.add_argument("--input", default="data/adr-0074-d6-baseline.jsonl",
                        help="ADR-0074 D6 JSONL 输入 (default: data/adr-0074-d6-baseline.jsonl)")
    parser.add_argument("--output", default="data/wave-3-training-data.jsonl",
                        help="输出 JSONL (default: data/wave-3-training-data.jsonl)")
    parser.add_argument("--source", default="baseline",
                        choices=sorted(VALID_SOURCES),
                        help="source 字段值 (default: baseline)")
    return parser.parse_args(argv)


def keep(record):
    """过滤条件: parse_valid == true && task_success == true.

    缺失字段默认 true — ADR-0074 V1 实际 schema {prompt, response, reward, metadata}
    无 parse_valid/task_success 字段, 缺失即视为兼容包含.
    """
    return record.get("parse_valid", True) is True and record.get("task_success", True) is True


def main(argv=None):
    args = parse_args(argv if argv is not None else sys.argv[1:])

    input_path = Path(args.input)
    if not input_path.is_file():
        print(f"ERROR: input not found: {input_path}", file=sys.stderr)
        return 1

    out_path = Path(args.output)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    written = 0
    with input_path.open("r", encoding="utf-8") as fin, \
         out_path.open("w", encoding="utf-8") as fout:
        for line in fin:
            line = line.strip()
            if not line:
                continue
            try:
                record = json.loads(line)
            except json.JSONDecodeError as exc:
                print(f"WARNING: skip malformed line: {exc}", file=sys.stderr)
                continue
            if not isinstance(record, dict):
                print("WARNING: skip non-object record", file=sys.stderr)
                continue
            if not keep(record):
                continue
            # 幂等: 已存在 source 字段则不覆盖 (design D3-2)
            record.setdefault("source", args.source)
            fout.write(json.dumps(record, ensure_ascii=False) + "\n")
            written += 1

    print(f"OK: wrote {written} records -> {out_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
