#!/usr/bin/env bash
set -euo pipefail
BIN="${1:-./build/neonstore}"
TMPDIR="$(mktemp -d)"
echo "Using BIN=$BIN TMP=$TMPDIR"
$BIN add-product --id P1 --name N --price 1.5 --stock 2 --dir "$TMPDIR"
$BIN list-products --dir "$TMPDIR" --output json > "$TMPDIR/list.json"
grep '"id":"P1"' "$TMPDIR/list.json" >/dev/null
$BIN sell --id P1 --qty 1 --dir "$TMPDIR"
$BIN save --dir "$TMPDIR"
$BIN load --dir "$TMPDIR"
echo "E2E OK"
