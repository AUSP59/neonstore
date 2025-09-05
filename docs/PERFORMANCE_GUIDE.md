# Performance Guide
- Use `-DCMAKE_BUILD_TYPE=Release -DENABLE_LTO=ON`.
- Consider PGO (`docs/PGO.md`) with representative workloads.
- Measure with `bench/bench_inventory.cpp` and track in CI artifacts.
- For profiling, use `perf` (Linux) or Instruments (macOS).