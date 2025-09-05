# CLI Usage

```bash
neonstore add --id P1 --name "Widget" --price 10.0 --stock 5
neonstore list
neonstore restock --id P1 --qty 3
neonstore sell --id P1 --qty 2
neonstore save --dir data
neonstore load --dir data
```

Use `--help` for commands and flags.


New commands: `stats`, `health`. New outputs: `--output tsv`. `--lang en|es` and env overrides.

Added commands: remove-product, set-price, find-product, export; and CLI E2E test.

`backup`, `restore`, `benchmark` added; list supports sort/filter/paging; import reads from stdin.

`list-orders`, `doctor`, `repair-integrity`, `--dry-run`, `--profile`, and `jsonl` output added.

`audit verify|show` and `config show` added. Mutations emit audit entries (append-only JSONL).

Added: `seed`, `audit rotate`, config auto-load precedence (CLI > env > file).

New: `diff`, `fingerprint`, `--anonymize` for export, `--read-only`, and `backup --keep N`.

Added: `sanitize`, policy flags (`--policy-*`), `restore --from latest`, and `config validate`.

New flags: `--integrity-algo crc32|sha256`, `--lock-mode dir|os`. `export` supports jsonl/tsv; `import` supports tsv.

Enhanced: `list-products` numeric filters & multi-key sort; `list-orders` filters; `trash list|undelete|purge`; backup MANIFEST with SHA-256.

New: `--expect-fingerprint` CAS guard; `snapshot` commands; `metrics`; `report sales`; `compact`.

New: `tx` transactions, `jsonrpc --stdio`, YAML output for listings, and `license check`.

New: `archive create|extract` (TAR), `dataset version show|set`, and `policy-check --policy-file FILE`.

New: `diff`, `redact`, `migrate plan|apply`, and `buildinfo`. Hooks: `.hooks/pre-save` & `.hooks/post-save`.

New: `diff --format patch` + `patch apply`, `audit rotate`, and `watch --native` (Linux).

New: `search-products`, `serve` (HTTP metrics), and `diff --orders`; packaging templates for Homebrew & Chocolatey.

New: `catalog generate`, `verify-all`, `snapshot prune --age-days`, `backup --max-total-bytes`, and `env`.

New: `bench`, `selftest`, `lint`, `stats`, and `help --markdown`. Locale forced to C.

New: `snapshot label`, `generate demo`, `diff --fail-on-diff`, global config (neonstore.ini), DRYRUN env var, and `help --json`.

New: `schema show|check`, `audit export|tail|grep`, `diff --orders-detail`, `config get|set`, and `stress`.

New: `transform-products`, `csv validate`, `trash purge --age-days`; CMakePresets and pre-commit helper.

New: `dataset lock|unlock|status`, `seal create|verify`, `perms check|fix`, `csv canonicalize`, and `transform-products --price-round`.

New: `quota set|status|unset`, `sidecars refresh`, `set-price-cas`, `backup verify`, `newid --format uuid4`, and `why`.

New: runtime policy constraints, `rename-products`, demo `--seed`, `audit rotate`, and `NEONSTORE_CHAOS_P`.

New: `doctor check|fix`, `policy init`, `batch apply`, and orders sorting in `csv canonicalize`.

New: `schema export`, `sidecars check|scrub`, `fs readonly`, and `retention enforce`.

New: `remove-product` (cascade), `export`/`import` JSONL, `lint ids`, and `price_change_max_pct` policy.

New: `dataset hash`, `orders summarize`, and `verify-all`. Added `-DNEONSTORE_STRICT=ON` option in CMake.

New: `lock`, `products stats`, `orders anomalies`, `index build|check|drop|lookup`, and `snapshot write`.

New: `txn status|recover`, `snapshot restore`, `version`, and shell completions.

New: `dataset diff`, `sbom generate`, `replicate push`, `search products|orders`, and `batch apply` idempotency via `op_id`.

New: `worm enable|status|disable`, `backup create|list|verify|restore`, `utf8 validate|scrub`, and `orders compact`.

New: top-level `vdiff`, `merge`, `schema-lint`, and `normalize`. Added build option `NEONSTORE_SANITIZE`.

New: `doctor fix`, `csv canonicalize`, `policy check`, and `fuzz run`. Atomic writes obtain a `.neonstore/writelock`.

New: `lint refs|csv`, `backup prune`, `config get|set|unset|list`, `audit enable|disable|status|tail`. Packaging with CPack.

New: `ledger enable|disable|status|tail|verify`, `export jsonl`, `report generate`, `search products --regex`, `health`.

New: `policy enforce`, `generate`, `dedupe products|orders`, `seal create|verify`, `audit rotate`, `ledger rotate`. Hardening flags documented in `HARDENING.md`.

New: `analytics summary`, `products set-price|set-stock`, `csv validate`, `audit prune`, `ledger prune`.

New: `query products|orders`, `mask products`, `bench run`, `doctor checkup`, `safemode`, `schema`.
