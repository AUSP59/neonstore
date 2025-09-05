# Snapshots

`snapshot write --out DIR` exports `products.jsonl`, `orders.jsonl`, and a `MANIFEST.json` with the
current `dataset_hash` (FNV‑1a 64-bit) for quick integrity checks.
