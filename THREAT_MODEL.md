# Threat Model (Local-First, No Network)

- **Adversary**: local user or malware attempting to tamper with dataset or releases.
- **Goals**: prevent silent corruption, enable detection and recovery.
- **Controls**: atomic writes with journal, WORM/LOCK, SEAL and tamper-evident ledger, CRC32/SHA-256 sidecars,
  verify-all and health checks, backups + restore, deterministic builds and SBOM.
- **Out of scope**: remote attackers (no network), cryptographic confidentiality (no encryption by design).
