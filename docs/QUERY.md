# Query DSL (Simple)

AND-only (`&&`) with operators `== != > < >= <=`; strings in double quotes.

```
# Products with price >= 10 and stock < 5
neonstore query products --where "price >= 10 && stock < 5" --order "price desc" --limit 20 --output json

# Orders for P1 with quantity >= 2
neonstore query orders --where "product_id == \"P1\" && quantity >= 2"
```
Fields:
- products: `id`, `name`, `price`, `stock`
- orders: `order_id`, `product_id`, `quantity`, `unit_price`
