# Coverage

Configure with `-DBUILD_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug`, then run `ctest` and collect with `gcovr`/`lcov`. Artifacts can be uploaded in CI (see `ci.yml`).