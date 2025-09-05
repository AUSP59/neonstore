#!/usr/bin/env bash
set -euo pipefail
BIN="${1:-./build/neonstore}"
TMP="$(mktemp -d)"
$BIN add-product --id P1 --name N --price 1.0 --stock 2 --dir "$TMP" >/dev/null
$BIN list-products --output json | grep '"id":"P1"' >/dev/null
$BIN set-price --id P1 --price 2.0 >/dev/null
$BIN find-product --id P1 --output json | grep '"price":2' >/dev/null
$BIN export --format csv --out "$TMP/out.csv"
test -s "$TMP/out.csv"
$BIN stats >/dev/null
$BIN validate --dir "$TMP" | grep '"ok":true' >/dev/null
echo "OK"
