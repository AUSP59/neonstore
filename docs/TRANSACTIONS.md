# Transactions & Recovery

Operations that change both `products.csv` and `orders.csv` use a **journaled two-phase write**:
1. Emit `products.csv.new` and `orders.csv.new`.
2. Write `txn_journal.json` with state `prepared`.
3. Rename `*.new` into place and remove the journal.

If a crash occurs, `neonstore txn recover` completes the pending rename(s). `neonstore txn status`
returns `pending` when a journal exists.
