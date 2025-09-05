# Dataset Hash

`neonstore dataset hash` computes a **non-cryptographic** FNV-1a 64-bit hash of the canonicalized
`products.csv` and `orders.csv` (CRLF normalized to LF). Use it to quickly detect changes between
datasets in CI and backups. For tamper-evidence, use seals and sidecars.
