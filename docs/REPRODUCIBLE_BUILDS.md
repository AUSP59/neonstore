# Reproducible Builds

This project supports reproducible builds via `-ffile-prefix-map/-fmacro-prefix-map` and the `SOURCE_DATE_EPOCH` convention.

## How to use
```bash
export SOURCE_DATE_EPOCH=$(date -ud "2020-01-01" +%s)
cmake -S . -B build -DENABLE_REPRODUCIBLE=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Artifacts produced will be byte-for-byte reproducible given identical toolchains.
