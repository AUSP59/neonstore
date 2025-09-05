# Release Checklist

1. `ctest` green across Linux/macOS/Windows (matrix).
2. `./build/neonstore verify-all --output text` OK on a clean dataset.
3. `./build/neonstore seal create` then `verify` OK.
4. Backups: `backup --dir data --out backups` then `backup verify --dir backups` OK.
5. LICENSE/SPDX headers and SBOM updated.
6. Changelog bump and tagged (SemVer).
