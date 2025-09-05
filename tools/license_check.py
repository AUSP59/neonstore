#!/usr/bin/env python3
import sys, pathlib, re
SPDX = re.compile(r"SPDX-License-Identifier:\s*Apache-2\.0")
bad = []
for p in pathlib.Path(".").rglob("*.*"):
    if p.suffix in (".cpp", ".hpp", ".h", ".c", ".cc"):
        try:
            head = p.read_text(encoding="utf-8", errors="ignore")[:300]
            if "SPDX-License-Identifier:" not in head:
                bad.append(str(p))
        except Exception:
            pass
if bad:
    print("Missing SPDX header in:", *bad, sep="\n")
    sys.exit(1)
print("All checked headers contain SPDX or are ignored.")
