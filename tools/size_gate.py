#!/usr/bin/env python3
import os, sys
path = sys.argv[1] if len(sys.argv)>1 else "./build/neonstore"
max_size = int(sys.argv[2]) if len(sys.argv)>2 else 15_000_000  # 15 MB
sz = os.path.getsize(path) if os.path.exists(path) else 0
print(f"Binary size: {sz} bytes (limit {max_size})")
sys.exit(0 if sz and sz <= max_size else 1)
