# Backups (TAR)

- `backup create --out DIR` → creates a `backup-<timestamp>.tar` with `products.csv`, `orders.csv`, and `MANIFEST.json` (dataset hash, version, timestamp).
- `backup list --dir DIR` → lists available backups.
- `backup verify --file FILE.tar` → recomputes dataset hash over embedded files and checks manifest.
- `backup restore --file FILE.tar` → **atomic** restore of both CSVs.
