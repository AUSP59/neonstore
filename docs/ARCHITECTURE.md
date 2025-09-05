# Architecture

- `neonstore` library (core domain): product, inventory, order, CSV storage.
- `neonstore` CLI: thin wrapper around the library.
- Separation of concerns: IO-free core where possible; CLI handles parsing/printing.

## Data Model
- Product: id, name, price, stock.
- Order/Item: id, product_id, quantity, price_each.

## Persistence
CSV files (`products.csv`, `orders.csv`) with simple escaping for commas and quotes.


## Thread-Safety
Inventory operations can be compiled with `-DENABLE_THREADSAFE=ON` (default) to guard mutations.
