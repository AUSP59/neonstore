# Lightweight Index (products.idx)

`index build` scans `products.csv` and writes `.neonstore/products.idx` mapping product ids to byte
offsets. Use `index lookup --id ID` to get the offset and `index check` for validation. The runtime
does not depend on the index; it is an optional operational accelerator.
