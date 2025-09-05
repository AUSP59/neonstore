# policy enforce

Reads `policy.ini` and **clamps** values in place (atomic write):
- `price_floor`, `price_ceiling` for products
- `max_order_item_qty` for order items

Use `--dry-run` to preview.
