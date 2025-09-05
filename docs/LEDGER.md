# Tamper-Evident Ledger

When enabled, each atomic dataset write appends a JSON-L line to `.neonstore/ledger.log`:

```json
{"ts":"2025-01-01T00:00:00Z","dataset":"<fnv64hex>","prev":"<64-hex>","entry":"<sha256(prev+dataset+ts)>"}
```

Commands:
```bash
neonstore ledger enable|disable|status|tail|verify
```
`verify` recomputes the chain and returns exit **0** when intact, **2** if tampering is detected.
