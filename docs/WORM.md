# WORM Mode (Write Once Read Many)

Enable a time-bound write lock that blocks all write commands:
```bash
neonstore worm enable --until 2030-01-01T00:00:00Z
neonstore worm status
neonstore worm disable
```
While WORM is active, only read-only commands are allowed.
