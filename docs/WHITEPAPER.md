# NeonStore: Design Whitepaper (Concise)

## Scope and Philosophy
NeonStore is a zero-dependency C++ library and CLI for local-first inventory & orders.
It favors simple, inspectable CSV storage with layered integrity (CRC32/SHA-256), HMAC audit,
and defensive operations (fsync, CAS, snapshots, backups with manifest, and doctor/repair).

## Storage
- CSV (RFC 4180) + sidecars: `.crc32` and `.sha256` for integrity. Audit log is append-only with hash chain (and optional HMAC).
- Concurrency: directory/OS locks, CAS fingerprint guard, thread-safe `Inventory`.
- Durability: atomic rename + directory fsync on POSIX; Windows uses appropriate APIs.

## Governance & Compliance
- SPDX headers, REUSE hints, SBOM, Scorecard/SLSA, security policy and CI enforcement.
- License, Code of Conduct, Contributing, Security, and Governance documents with review/PR templates.

## Operations
- Backups with `MANIFEST.txt` (SHA-256), snapshots, compact, policy-check (file-based), doctor and repair.
- Reproducibility: Nix flake, deterministic release knobs, hardening flags, LTO & section-GC.

## Extensibility
- JSON-RPC over stdio for local IPC; scripting-friendly outputs (JSON/JSONL/YAML/TSV).
- Hooks on save: `.hooks/pre-save` and `.hooks/post-save` (env: `NEONSTORE_DIR`, `NEONSTORE_HOOK_TARGET`).

## Migration & Lifecycle
- Dataset version marker and migration helpers to evolve data formats in small, auditable steps.
