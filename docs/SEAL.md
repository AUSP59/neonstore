# seal

Creates/verifies a dataset **seal** (tamper evidence snapshot):

```bash
neonstore seal create
neonstore seal verify   # exit 0 = ok, 2 = mismatch
```
The seal stores a SHA-256 over canonicalized `products.csv` + `orders.csv`.
