#!/usr/bin/env bash
set -euo pipefail
shopt -s nullglob
for f in "$@"; do
  sha256sum "$f"
done > SHA256SUMS.txt
echo "Generated SHA256SUMS.txt"
