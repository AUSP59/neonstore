# Advanced Policies

- `price_change_max_pct=NN.N` — maximum allowed price change in percent for *any* write path that updates prices (applies to `set-price`, `transform-products`, and `batch apply`). Set to `-1` to disable. You can also set `NEONSTORE_PRICE_CHANGE_MAX_PCT` env var.
- `product_id_regex=^[A-Z0-9_-]+$` — optional regex to validate product ids.
- `order_id_regex=^[A-Z0-9_-]+$` — optional regex to validate order ids.
