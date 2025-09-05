# Locks

- `lock on|off|status` toggles/inspects dataset lock by creating/deleting a `LOCKED` file.
- Environment `NEONSTORE_LOCK=1` also forces read-only operation.
- `why` prints the effective lock/quota/policy status.
