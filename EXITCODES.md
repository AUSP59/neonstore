# Exit Codes

- `0` — success.
- `1` — usage error / invalid arguments.
- `2` — validation/check failure (schema, verify, csv validate, seal verify).
- `3` — diff detected (when `--fail-on-diff`) or CAS mismatch.
- `4` — dataset locked / read-only mode.
- `5` — blocked by runtime policy (deny list).

Other operations may emit structured messages (`policy_violation`, `quota_exceeded`, `chaos_injected`) and return `0` when the write was skipped; combine with `why`, `quota status`, and logs for CI enforcement.
