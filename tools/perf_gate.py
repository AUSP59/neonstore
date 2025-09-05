#!/usr/bin/env python3
import subprocess, sys, re, json
# Run bench and parse 'Inserted: N in X ms, list in Y ms' lines
cmd = ["./build/bench/bench_inventory"]
try:
    out = subprocess.check_output(cmd, stderr=subprocess.STDOUT, text=True, timeout=120)
except Exception as e:
    print("Failed to run benchmark:", e); sys.exit(2)
m = re.search(r"Inserted:\s+(\d+)\s+in\s+(\d+)\s+ms,\s+list\s+in\s+(\d+)\s+ms", out)
if not m:
    print("Benchmark output not parseable"); sys.exit(2)
N, insert_ms, list_ms = map(int, m.groups())
print(json.dumps({"N":N, "insert_ms":insert_ms, "list_ms":list_ms}))
# Budget gate (defaults)
max_insert = int(sys.argv[1]) if len(sys.argv)>1 else 2500
max_list = int(sys.argv[2]) if len(sys.argv)>2 else 2500
if insert_ms > max_insert or list_ms > max_list:
    print(f"Perf gate failed: insert {insert_ms}>{max_insert} or list {list_ms}>{max_list}")
    sys.exit(1)
print("Perf gate OK")
