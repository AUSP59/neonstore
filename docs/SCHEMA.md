# Schema & Migrations

Schema file: `.neonstore/schema.json` with fields `name` and `version`.

```
neonstore schema init
neonstore schema bump --to 2
neonstore schema migrate
```
`migrate` currently re-writes the canonical CSV and refreshes sidecars.
