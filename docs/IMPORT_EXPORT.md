# Import / Export

## Export
```bash
neonstore export products --format jsonl > products.jsonl
neonstore export orders --format jsonl > orders.jsonl
```
- `jsonl` emits one JSON object per line.
- `csv` emits RFC 4180 CSV.

## Import
```bash
neonstore import products --from products.jsonl
```
- Only **products** are supported for import for now (id, name, price, stock).
- Import is idempotent: existing ids are updated; missing ids are created.
- Input must be valid JSONL like:
```
{"id":"P1","name":"Widget","price":9.99,"stock":5}
{"id":"P2","name":"Gizmo","price":14.50,"stock":10}
```
