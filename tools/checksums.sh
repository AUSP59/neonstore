#!/usr/bin/env bash
set -euo pipefail
for f in "$@"; do
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$f"
  elif command -v shasum >/dev/null 2>&1; then
    shasum -a 256 "$f"
  else
    echo "No SHA256 tool found" >&2; exit 1
  fi
done
