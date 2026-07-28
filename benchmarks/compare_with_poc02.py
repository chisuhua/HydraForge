#!/usr/bin/env python3
"""Compare HydraForge Temporal Agent history_size_bytes vs PoC-02 Python baseline.

Usage:
  python benchmarks/compare_with_poc02.py

This script writes a baseline JSON file consumed by the C++ test
test_temporal_agent_history_baseline.  When a real PoC-02 repo and Temporal
dev server are available, update the values below with actual measurements.

Requirements for real benchmark:
  - Temporal dev server running (temporal server start-dev)
  - GrpcTemporalBackend connecting to localhost:7233
  - PoC-02 Python repo at /workspace/poc02-temporal/ for baseline computation
"""

import json
import os
import sys

# Placeholder baseline from PoC-02 docs (update with actual measurement)
poc02_baseline = {
    "workflow": "identical_workflow_5_steps",
    "iterations": 10,
    "avg_history_bytes": 4523,
    "note": (
        "Placeholder value.  Replace with actual average after running "
        "PoC-02 10x and measuring history_size_bytes via GetWorkflowExecutionHistory."
    ),
}

out_path = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                         "poc02_baseline.json")
with open(out_path, "w") as f:
    json.dump(poc02_baseline, f, indent=2)

print(f"Wrote placeholder baseline to {out_path}")
print("Run actual benchmark when gRPC dev env is available")
print("  1. cmake -DTEMPORAL_ENABLE_GRPC=ON -DTEMPORAL_HISTORY_BASELINE_ENABLED=ON ..")
print("  2. temporal server start-dev &")
print("  3. ctest -R history_baseline --output-on-failure")
