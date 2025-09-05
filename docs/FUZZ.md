# Fuzz Runner

Runs randomized operations (bounded and policy-aware) to sanity-check invariants.

```bash
neonstore fuzz run --ops 2000 --seed 42
```
Reports counts and returns non-zero if basic invariants are violated.
