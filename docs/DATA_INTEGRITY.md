# Data Integrity & Backups
- CSV sidecar CRC32 (`*.crc32`) verifies on load; set `--integrity strict` to enforce.
- `backup --dir DATA --out BACKUPS/` writes timestamped copies of `products.csv*` and `orders.csv*`.
- `restore --dir DATA --from BACKUPS/` restores the latest selected backup.
