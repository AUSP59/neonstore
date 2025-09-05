# doctor fix

Performs an in-place, **atomic** cleanup:
- Canonicalizes CSV ordering (products by `id`, orders by `order_id`, items by `product_id`).
- Regenerates sidecars: `.crc32` and `.sha256` for `products.csv` and `orders.csv` (CRLF->LF canonicalized contents).

```bash
neonstore doctor fix
```
Exit 0 on success.
