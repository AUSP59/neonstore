# Changelog

All notable changes are documented here. This project follows [Keep a Changelog](https://keepachangelog.com/) and [SemVer](https://semver.org/).

## [Unreleased]

## [1.0.0] - 2025-09-04
- Initial public release of NeonStoreSystem (World-Class C++ OSS Edition)


## [1.1.0] - 2025-09-04
### Added
- Robust CSV parsing & atomic saves, JSON output for `list-products`.
- CPack packaging, CodeQL & Release workflows.
- New docs: SUPPORT, ROADMAP, NOTICE, STYLE, ETHICS, PRIVACY, CLI, OSS_COMPLIANCE, REPRODUCIBLE_BUILDS.
- SBOM (CycloneDX) artifact included by default.
- Benchmarks & fuzz targets (build-time optional).

### Fixed
- Completed `main.cpp` and `csv_storage.cpp` implementations.

## [2.0.0] - 2025-09-04
### Added
- **Storage abstraction** with CSV adapter and optional **SQLite** backend.
- **Config file** support, verbosity and no-color flags.
- **Hardening flags**, PIE, RELRO/NOW, optional **LTO**, and **PCH**.
- **E2E** script and manpage; extended **fuzzing** for orders.
- **JSON Schemas** for machine-readable outputs.

## [2.1.0] - 2025-09-04
### Added
- Proper JSON escaping, env var overrides, shell completions, manpage install.
- CMake **package config** for consumers; `find_package(NeonStoreSystem)` now works.
- **SLSA provenance** workflow (template), Homebrew & Scoop packaging templates.
- CI: clang-format check, ThreadSanitizer job, coverage **gate** (80%).  
- Doxygen target and SPDX headers across sources.

## [2.2.0] - 2025-09-04
### Added
- **Export macros** and default-hidden symbols; versioned **shared library**.
- **pkg-config** file installation, cross-compilation toolchains, **nix flake**.
- **validate** command and CSV edge-case tests; fuzz seed corpora.
- **Per-instance** thread-safety (replaces global guard).
- **NO_COLOR** env respected; DEB/RPM CPack metadata; devcontainer & presets retained.

## [2.3.0] - 2025-09-04
### Added
- Inventory ops: `remove_product`, `set_price`; CLI: `remove-product`, `set-price`, `find-product`, `export`.
- Auto-discovered CTest targets and UNIX CLI smoke test.
- Reproducible builds respect `SOURCE_DATE_EPOCH` and `-ffile-prefix-map`.
- DCO workflow and Dependabot for actions.

## [2.4.0] - 2025-09-04
### Added
- **C API** (`neonstore_c`) with minimal surface and ELF symbol versioning map.
- **Optional Python bindings** (pybind11) guarded by `-DENABLE_PYTHON=ON`.
- Arg parser soporta `--key=value` y valores con comillas.
- **Concurrency test** (threads), **REUSE** lint y **Trivy** scan en CI.
- Completions actualizadas, sample config, `.editorconfig`.

## [2.5.0] - 2025-09-04
### Added
- LANG auto-detection (defaults to Spanish if `LANG` starts with `es`), override with `--lang`.
- New CLI commands: `schema` (prints ProductList JSON Schema) and `config generate` (writes a sample config).
- Tooling configs: **.clang-format** and **.clang-tidy**.
- CI jobs: include-what-you-use, clang **scan-build**, **hardening** check, and **pre-commit** runner.

## [2.6.0] - 2025-09-04
### Added
- Logging facility (opt-in via `--verbose`) and CSV directory locking for concurrent safety.
- CLI: `import --format csv --file FILE` and `migrate` between backends.
- Build: `WARNINGS_AS_ERRORS` option (default ON) with -Wall/-Wextra/-Wpedantic.
- CI: **Valgrind** job for memory checking on Linux.

## [2.7.0] - 2025-09-04
### Added
- **Integrity** for CSV via CRC32 sidecar files; `--integrity off|lenient|strict` (default: lenient).
- **Quiet mode** (`--quiet`) to suppress normal stdout; errors still print to stderr.
- **Pagination** for `list-products` with `--limit/--offset`.
- **ID pattern** validation in `add-product` via `--id-pattern REGEX`.
- **SQLite** saves now run inside transactions and enable `PRAGMA foreign_keys=ON`.
- **ABI dump** job in CI (abi-dumper) to track binary interface evolution.
- Test: `test_integrity.cpp` for strict integrity enforcement.

## [2.8.0] - 2025-09-04
### Added
- CLI: sorting (`--sort`), descending (`--desc`), filtering (`--filter`/`--filter-regex`), paging (`--limit/--offset`) in `list-products`.
- CLI: `backup`, `restore`, `benchmark`; `import` now supports stdin via `--file -`.
- CSV: sets owner-only permissions on POSIX data dir after creation.
- Build: `EXTRA_WARNINGS` (default ON) adds `-Wconversion -Wsign-conversion -Wshadow -Wdouble-promotion` on non-MSVC.

## [2.9.0] - 2025-09-04
### Added
- `--dry-run` and `--profile` flags for mutating commands.
- `list-orders` (text/json/tsv), `jsonl` output for `list-products`.
- `doctor` and `repair-integrity` commands for troubleshooting and CRC sidecar rebuild.
- POSIX guard against symlinked data directories (override via `NEONSTORE_ALLOW_SYMLINKS=1`).
- Forces locale to "C" at startup for consistent decimal parsing.

## [2.10.0] - 2025-09-04
### Added
- **Append-only audit log** with CRC32 hash chaining (`audit.log`); `audit verify|show` commands.
- **Disk space preflight** on CSV saves (throws if insufficient space for atomic rename).
- **config show** prints effective backend/dir/dsn (CLI/env).

## [2.11.0] - 2025-09-04
### Added
- **Durability**: `fsync` after CSV saves and audit appends (Windows `_commit`).
- **Config auto-load** via `--config`, `NEONSTORE_CONFIG`, or XDG `~/.config/neonstore/neonstore.conf` (merged with CLI).
- **Audit**: `audit rotate` command; **seed** generator (`seed --products N --prefix P`).
- **CI**: coverage.xml artifact job; reproducible-build check (stable `SOURCE_DATE_EPOCH`).
- **Repo hygiene**: `.pre-commit-config.yaml` and `CODEOWNERS`.

## [2.12.0] - 2025-09-04
### Added
- Commands: `diff`, `fingerprint` (SHA-256), and `seed` retained.
- Export: `--anonymize` masks IDs/names.
- Safety: `--read-only` rejects mutations; `backup --keep N` prunes old backups.
- Tooling: Dockerfile, Makefile, Homebrew & Scoop templates; zsh/fish completions.
- Tests: fingerprint determinism.

## [2.13.0] - 2025-09-04
### Added
- **Policy enforcement** for IDs/prices/stocks via CLI/env/config.
- **sanitize** command for data hygiene; **restore latest** convenience; **config validate** sanity checker.
- **Color env**: honors CLICOLOR/CLICOLOR_FORCE alongside NO_COLOR.
- CI: **cppcheck** job with artifact upload.

## [2.14.0] - 2025-09-04
### Added
- Interactive `shell`, `--color` flag with TTY/env awareness, and `OrderList` JSON Schema.
- Man pages (`neonstore.1`, `neonstore-config.5`), Nix flake, and examples for C/Python/Rust.
- CI: **SBOM** (CycloneDX) and **coverage gate >=85%**.

## [2.15.0] - 2025-09-04
### Added
- Integrity: SHA-256 sidecars selectable via `--integrity-algo` (CRC32 remains default for speed).
- Locking: OS-level file lock option `--lock-mode os` (POSIX fcntl / Windows LockFileEx).
- Concurrency: Inventory now guarded by a mutex for thread safety.
- Import/Export: `export` supports **jsonl** and **tsv**; `import` accepts **tsv**.
- Audit: optional **HMAC-SHA256** (`NEONSTORE_AUDIT_HMAC_KEY`) enforced by `audit verify` if present.
- Build: **ENABLE_LTO** and section GC; ops templates for systemd/Task Scheduler.

## [2.17.0] - 2025-09-04
### Added
- `list-products`: numeric filters (`--price-gt/lt`, `--stock-gt/lt`) and multi-key `--sort`.
- `list-orders`: `--min-total/--max-total` and `--filter`.
- `trash` subcommands and automatic `.trash.csv` on removals.
- `backup`: `MANIFEST.txt` with SHA-256 sums.
- Packaging: install rules for man/completions; Debian & RPM skeletons.

## [2.18.0] - 2025-09-04
### Added
- **CAS guard** via `--expect-fingerprint HEX` on mutating commands to prevent lost updates.
- **POSIX directory fsync** after atomic renames in CSV saves (stronger durability).
- **Snapshots** under `.snapshots/` with create/list/restore/prune.
- **Metrics** output (Prometheus or JSON) and **sales reports** (top N by revenue/quantity).
- **Compact** command to sort and deduplicate products file.

## [2.19.0] - 2025-09-04
### Added
- Transactions: `tx begin|status|run -- <args...> | commit | abort` for atomic batch edits.
- JSON-RPC over stdio: `jsonrpc --stdio` with minimal methods (`ping`, `listProducts`, `fingerprint`).
- Output: YAML support in `list-products` and `list-orders`.
- Compliance: `license check` for SPDX header coverage.

## [2.20.0] - 2025-09-04
### Added
- `archive create|extract` for TAR archives of the data dir (offline portability).
- `dataset version show|set` to manage a DATASET_VERSION marker.
- `policy-check --policy-file FILE` to validate against a JSON policy bundle.
- Build hardening flags and CI **clang-format** check.

## [2.21.0] - 2025-09-04
### Added
- **Diff** tool for datasets (`diff --left --right` with text/json/yaml).
- **Redaction** pipeline (`redact`) with pseudonymization & hashed IDs (orders preserved).
- **Migrations** (`migrate plan|apply`) with example steps 1→2 (price rounding) and 2→3 (uppercase IDs).
- **Hooks** on save: `.hooks/pre-save` and `.hooks/post-save` with env variables.
- **Buildinfo** command (JSON/text) and build macro defs via CMake.
- **Whitepaper** (`docs/WHITEPAPER.md`).

## [2.22.0] - 2025-09-04
### Added
- `diff --format patch` and `patch apply` for offline, deterministic changesets.
- `audit rotate --max-size --keep` for log retention.
- `watch --native` (Linux inotify) with graceful fallback to polling.
- Hooks can be globally disabled via `NEONSTORE_NO_HOOKS=1`.
- Documented standardized exit codes.

## [2.23.0] - 2025-09-04
### Added
- Fuzzy search: `search-products --query Q [--max N] [--fuzzy]` (Levenshtein ranking).
- HTTP metrics server: `serve --port N [--bind HOST]` with `/metrics` and `/healthz` (no external deps).
- Diff enhancements: `diff --orders` to summarize order IDs changes.
- Packaging templates: Homebrew formula and Chocolatey nuspec.
### Changed
- `newid` ULID randomness now uses `std::random_device` + `mt19937_64` for better entropy.

## [2.24.0] - 2025-09-04
### Added
- `catalog generate` summarizes dataset state & integrity in one command.
- `verify-all` validates sidecars, audit chain and referential integrity (exit code 2 on failure).
- `snapshot prune --age-days` for age-based retention; `backup --max-total-bytes` to cap total backup size.
- `env` prints effective environment & build metadata.
- PowerShell completion script and CI build matrix across Linux/macOS/Windows.
- Threat model doc and CODEOWNERS.

## [2.25.0] - 2025-09-04
### Added
- `bench` micro-benchmark (JSON output) and `selftest` quick integrity verifier.
- `lint` dataset linter and `stats` analytics.
- `help --markdown` for docs automation.
- Bash/Zsh completion scripts.
### Changed
- Numeric locale is forced to **C** at startup to ensure reproducible I/O across locales.

## [2.26.0] - 2025-09-04
### Added
- Snapshot labels (`snapshot label <TS> --label L`) and label-aware `snapshot list`.
- Demo dataset generator (`generate demo --products N --orders M`).
- `diff --fail-on-diff` (exit code 3 on differences).
- Global config (`neonstore.ini`, `.neonstore/config.ini`, or XDG) for default data dir.
- `NEONSTORE_DRYRUN=1` to skip writes safely.
- `help --json` for CLI introspection; fish completions.
- Governance docs: `MAINTAINERS.md`, `SUPPORT.md`, and `CITATION.cff`.
### Changed
- `buildinfo` exposes `SOURCE_DATE_EPOCH` when provided.
- CMake options to enable sanitizers at build time.

## [2.27.0] - 2025-09-04
### Added
- `schema show|check` for header verification.
- `audit export|tail|grep` (text/JSONL) to work with the audit log.
- `diff --orders-detail` JSON enrichments for orders.
- `config get|set [--global]` for writing/reading neonstore.ini.
- `stress` temp-dir concurrency exerciser.
- `.editorconfig` and `.gitattributes` for consistent dev experience.

## [2.28.0] - 2025-09-04
### Added
- `transform-products` for safe bulk price/stock updates with filters and JSON summary.
- `csv validate` to detect header/column/type issues (exit 2 on errors).
- `trash purge --age-days` to age-prune trashed entries.
- `CMakePresets.json` and `tools/pre-commit.sh` for faster local workflows.

## [2.29.0] - 2025-09-04
### Added
- Global write guard: `dataset lock|unlock|status` (LOCKED sentinel + CLI enforcement).
- Tamper-evident `seal create|verify` using dataset fingerprint.
- Permissions tooling: `perms check|fix` (POSIX) and secure perms on saves.
- `csv canonicalize` for stable ordering; `transform-products --price-round` for precise pricing.

## [2.30.0] - 2025-09-04
### Added
- Runtime policy guard via `policy.ini` (deny list) and `why` inspector.
- Quotas: `quota set|status|unset` with write enforcement.
- `sidecars refresh` to recompute integrity files.
- `newid --format uuid4` and `newid-uuid4` alias.
- `set-price-cas` (compare-and-swap) to prevent lost updates.
- `backup verify` to sanity-check MANIFEST.
- Docs: Accessibility, D&I, Sustainability, Ethics, Release checklist; GitHub issue/PR templates.

## [2.31.0] - 2025-09-04
### Added
- Runtime policy limits (price/stock/quantity/name) enforced on writes.
- `rename-products` (regex-based), `generate demo --seed S`, and `audit rotate` with retention.
- Chaos testing via `NEONSTORE_CHAOS_P`.
- Packaging with CPack (TGZ) and CodeQL workflow.
- EXITCODES, minimal man page, and example dataset.

## [2.32.0] - 2025-09-04
### Added
- `doctor check|fix` for health checks and simple repairs.
- `policy init` to scaffold runtime policies.
- `batch apply` for atomic multi-op edits to products.
### Changed
- `csv canonicalize` sorts **orders** by `order_id` for deterministic diffs.

## [2.33.0] - 2025-09-04
### Added
- `schema export --format json|md` for machine-readable schema.
- `sidecars check|scrub` to detect and clean missing/orphan sidecars.
- `fs readonly` to detect directory writability.
- `retention enforce` for audit and backup pruning by age.
- Tooling: `.clang-format` and `.clang-tidy`.

## [2.35.0] - 2025-09-04
### Added
- `remove-product --cascade` with `--dry-run` and referential-safety guarantees.
- `export products|orders` to JSONL/CSV; `import products` from JSONL.
- `lint ids` validates ids with policy regex.
- Policy `price_change_max_pct` enforced across write paths.
### Fixed
- Ensured **English-only** documentation sections; added "Documentation Language" note.

## [2.36.0] - 2025-09-04
### Added
- `dataset hash` (FNV‑1a 64-bit), `orders summarize`, and `verify-all` orchestrator.
- CMake option `NEONSTORE_STRICT` for stricter warnings across compilers.

## [2.37.0] - 2025-09-04
### Added
- Lock management CLI; `products stats`; `orders anomalies` with policy-driven tolerance.
- Lightweight product index: build/check/drop/lookup.
- `snapshot write` for JSONL exports + dataset hash manifest.
- Tooling: EditorConfig, gitattributes, pre-commit; CI workflow for build/test (3 OS).

## [2.38.0] - 2025-09-04
### Added
- Journaled two-phase dataset writes with `txn status|recover`.
- `snapshot restore --from DIR` with atomic commit.
- `version` command; shell completions; install targets.
- Basic CTest cases; CODEOWNERS and DCO.

## [2.39.0] - 2025-09-04
### Added
- `dataset diff`, `sbom generate`, `replicate push`, `search` helpers.
- Idempotency for `batch apply` via `op_id` log.

## [2.40.0] - 2025-09-04
### Added
- WORM mode with time-bound write guard across commands.
- TAR backups: create/list/verify/restore with dataset hash manifest and atomic restore.
- UTF-8 validation and scrubbing for product names.
- `orders compact` for merging duplicates and dropping zero-quantity items.
### Docs
- Added BACKUPS.md, WORM.md, and installed `man/neonstore.1`.

## [2.41.0] - 2025-09-04
### Added
- Top-level commands: `vdiff` (verbose diff), `merge` (dataset merge with strategies, dry-run), `schema-lint`, `normalize`.
### Build
- Option `NEONSTORE_SANITIZE` to enable ASan/UBSan when supported.
### Tooling
- Expanded pre-commit hooks: detect-private-key & check-added-large-files.

## [2.42.0] - 2025-09-04
### Added
- `doctor fix` (canonicalize + regenerate sidecars).
- `csv canonicalize` for deterministic CSV ordering.
- `policy check` (price floor/ceiling, max order item qty).
- `fuzz run` randomized validator.
### Changed
- Atomic writer acquires a repo-local write-lock to prevent concurrent writers.

## [2.43.0] - 2025-09-04
### Added
- `lint refs|csv` for referential integrity and numeric sanity checks.
- `backup prune --keep N` retention helper.
- `config get|set|unset|list` key-value store under `.neonstore/`.
- `audit enable|disable|status|tail` for command-level audit entries.
- CPack TGZ packaging and CI packaging step.
### Docs
- SECURITY.md, SUPPORT.md, GOVERNANCE.md, WHITEPAPER.md; docs for new commands.
- `.clang-format` style config.

## [2.44.0] - 2025-09-04
### Added
- Tamper-evident **ledger** with hash-chained entries and `verify` command.
- `export jsonl` for products/orders.
- `report generate` static HTML summary.
- `search products --regex` and `health` quick summary.

## [2.45.0] - 2025-09-05
### Added
- `policy enforce` with `--dry-run`.
- `generate` deterministic dataset generator.
- `dedupe products|orders` utilities.
- `seal create|verify` (dataset seal).
- `audit rotate`, `ledger rotate`.
### Build
- Optional hardening flags via `-DNEONSTORE_HARDEN=ON`.
### CI
- clang-format style check step.

## [2.46.0] - 2025-09-05
### Added
- Analytics KPIs (`analytics summary`) and CSV validation (`csv validate`).
- Targeted edits: `products set-price|set-stock`.
- Man page and shell completions; warnings-as-errors option.
- Audit/Ledger prune; additional governance docs (Privacy, Threat Model, Supply Chain).
### Tooling
- .clang-tidy baseline.

## [2.47.0] - 2025-09-05
### Added
- Query DSL with ordering and limit.
- Product masking (redact/hash).
- Benchmark and Doctor checkup.
- Safe mode (read-only) enforced at write path.
- Schema file and simple migrations.
