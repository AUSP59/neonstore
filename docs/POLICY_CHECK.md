# policy check

Validates dataset conformance to simple numeric policies:

Supported keys in `policy.ini` (any of: repo root, `.neonstore/`, or dataset dir):
- `price_floor=<number>`
- `price_ceiling=<number>`
- `max_order_item_qty=<integer>`

Exit 0 when OK, 2 when violations are found.
