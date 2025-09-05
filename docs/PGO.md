# Profile-Guided Optimization (PGO)

1. Build with instrumentation: `-DENABLE_PGO=ON -DCMAKE_BUILD_TYPE=Release`.
2. Run representative workloads (CLI over your dataset).
3. Rebuild to consume profiles. (Exact flags depend on your compiler toolchain.)