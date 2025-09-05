# verify-all

Runs an all-in-one health check:
- Doctor-style validations (referential integrity and value ranges).
- Sidecar presence (missing/orphan).
- Dataset hash report.

Exit code: `0` when all OK, `2` if any problem is detected.
