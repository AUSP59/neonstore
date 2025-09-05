# schema-lint

Validates CSV headers exactly match expected columns:
- products.csv: `id,name,price,stock`
- orders.csv: `order_id,product_id,quantity,unit_price`
Exit 0 when OK; 2 when mismatched.
