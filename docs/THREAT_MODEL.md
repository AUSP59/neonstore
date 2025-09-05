# Threat Model (Concise)

## Assets
- Product & order data (CSV), audit log with HMAC option, backups & snapshots, sidecars (.crc32/.sha256).

## Trust Assumptions
- Single-machine, local-first. Users control filesystem and execution environment. No network dependencies by default.
- Attackers may have limited local access but not root/admin.

## Risks & Mitigations
- **Data corruption**: mitigated by CRC32/SHA-256, fsync, atomic rename, directory fsync, `verify-all`, backups/snapshots, `compact`.
- **Tampering**: audit hash-chain with optional HMAC, `verify-all` to detect mismatches.
- **Race conditions**: file locking, **CAS guard**, TX staging dir, `snapshot` safe points.
- **Supply chain**: SBOM, REUSE/SPDX, CI checks, hardening flags, build matrix, provenance docs, `buildinfo`.
- **Exfiltration via hooks**: hooks opt-in, can be disabled via `NEONSTORE_NO_HOOKS=1`.
- **Misconfiguration**: `catalog` summarizes health; `policy-check` enforces dataset rules; `OPERATIONS.md` documents exit codes.

## Out-of-Scope
- At-rest encryption and RBAC (need external key management). Networked multi-user concurrency.
