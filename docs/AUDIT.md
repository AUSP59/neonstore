# Audit Log

Enable/disable a simple audit log that records command invocations (best-effort):
```bash
neonstore audit enable
neonstore audit status
neonstore audit tail
neonstore audit disable
```
Log file: `.neonstore/audit.log`. Entries include UTC timestamp and full argv.
