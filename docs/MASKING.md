# Masking / Anonymization

```
neonstore mask products --strategy redact --out masked --dir data
neonstore mask products --strategy hash   --out masked --dir data
```
Redact replaces names with `REDACTED-<hash>`. Hash generates a stable pseudonym.
Orders are copied verbatim.
