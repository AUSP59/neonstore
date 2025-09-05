# merge (Dataset Merge)

```bash
neonstore merge --dir2 other_dir [--strategy prefer-dir1|prefer-dir2|safest] [--dry-run]
```
- Products: creates missing; resolves conflicts per strategy (`safest` = min price, max stock).
- Orders: union by `order_id`; items with same `product_id` sum quantities.
- Writes are **atomic**. Use `--dry-run` to preview.
