# Doctor Checkup

Aggregate health check:
```
neonstore doctor checkup --dir data --output json
```
Reports missing references, bad rows, and (if present) SEAL and ledger status.
Exit 0 if all OK, 2 otherwise.
