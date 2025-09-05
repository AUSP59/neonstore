# csv canonicalize

Writes canonical CSVs deterministically:
- `products.csv` sorted by `id`
- `orders.csv` with orders by `order_id` and items by `product_id`
Uses atomic dataset write.
