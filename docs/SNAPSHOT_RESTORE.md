# Snapshot Restore

`neonstore snapshot restore --from DIR` reads `products.jsonl` and `orders.jsonl` and performs an
**atomic dataset write** using the transaction mechanism. Use together with `snapshot write` for
robust backup/restore flows without external dependencies.
