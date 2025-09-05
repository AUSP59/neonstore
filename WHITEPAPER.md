# NeonStore: A Deterministic, Local-First Store Dataset Engine (Whitepaper)

**Abstract.** NeonStore is a zero-dependency, local-first engine for store catalogs and orders,
prioritizing determinism, auditability, and operational safety. It uses CSV as an open, portable
storage format and layers integrity sidecars, policies, and transactional writes for production use.

## 1. Design Principles
- Determinism over cleverness; atomicity over partial writes; explicit policies over magic.

## 2. Data Model
- `products.csv (id,name,price,stock)`; `orders.csv (order_id,product_id,quantity,unit_price)`.
- Referential integrity is enforced by `lint refs` and in `verify-all` flows.

## 3. Integrity
- Canonical line endings; FNV-1a dataset hash; sidecars with CRC32/SHA-256; dataset seals and WORM.

## 4. Transactions
- Journaled two-phase commit, crash recovery, write lock and WORM guard.

## 5. Operations
- Snapshots and TAR backups (hash-manifest), restore is atomic; merge/diff/vdiff; anomalies detection;
  normalization and UTF‑8 hygiene; batch idempotency; replication; backups prune.

## 6. Supply Chain
- Reproducible builds knobs, CI on 3 OS, CodeQL/Scorecard ready, SPDX/REUSE friendly.

## 7. Limitations
- CSV scaling and lack of indexes vs. embedded DBs; optional lightweight index provided.
- No network or external deps by design.

## 8. Future Work
- Optional modules (SQLite/yyjson/cURL) for richer queries and federation (opt-in).
