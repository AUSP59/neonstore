### Exit codes
- 0: success
- 1: usage error or generic failure
- 2: data/row validation failure (strict import, etc.)
- 4: write blocked (read-only mode)
- 6: CAS guard/fingerprint mismatch
- 130: interrupted (SIGINT)
- 143: terminated (SIGTERM)
