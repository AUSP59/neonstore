# Exit Codes (Stable)

- `0`  → Success
- `1`  → Usage / invalid arguments
- `2`  → Lint/validation failures (schema, refs, csv, policy, vdiff mismatch)
- `4`  → Write disallowed by policy/lock (e.g., WORM, read-only env)
- `6`  → CAS guard mismatch or dataset changed concurrently
- `13` → WORM guard preventing write
