# Safe Mode (Read-Only)

Enable to prevent any write (atomic writes refuse with `read_only`):
```
neonstore safemode enable --dir data
neonstore safemode status  # on
neonstore products set-price --id P1 --price 9.99 --dir data  # will fail
neonstore safemode disable --dir data
```
