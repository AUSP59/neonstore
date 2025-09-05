# dedupe

- `dedupe products` removes duplicate product IDs (keeps first occurrence).
- `dedupe orders` merges duplicated order lines (same `order_id`,`product_id`) by summing quantities.

Supports `--dry-run`.
