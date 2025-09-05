# Stats & Reports

- `products stats` → count, min/max/avg price, total stock, inventory value.
- `orders summarize` → orders/items/units, revenue, AOV, and top-N products by revenue.
- `orders anomalies` → detects unit prices deviating beyond a tolerance from current product price.
  - Tolerance from `--delta-pct` or `unit_price_delta_pct` policy (default 10%).
