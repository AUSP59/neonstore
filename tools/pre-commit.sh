#!/usr/bin/env bash
set -euo pipefail
echo "[pre-commit] format check (if available)"
if command -v clang-format >/dev/null 2>&1; then
  git ls-files '*.cpp' '*.hpp' | xargs -r clang-format -i
fi
echo "[pre-commit] configure + build + test"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
echo "[pre-commit] lint + selftest"
./build/neonstore lint || true
./build/neonstore selftest || true
echo "[pre-commit] done"
