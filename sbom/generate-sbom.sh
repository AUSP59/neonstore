#!/usr/bin/env bash
set -euo pipefail
OUT_DIR="sbom"
mkdir -p "$OUT_DIR"
if command -v syft >/dev/null 2>&1; then
  syft dir:. -o cyclonedx-json > "$OUT_DIR/sbom.cdx.json"
  echo "SBOM -> $OUT_DIR/sbom.cdx.json"
else
  cat > "$OUT_DIR/sbom.spdx" <<EOF
SPDXVersion: SPDX-2.3
DataLicense: CC0-1.0
SPDXID: SPDXRef-DOCUMENT
DocumentName: neonstoresystem
DocumentNamespace: https://example.org/spdx/neonstoresystem-$(date +%s)
Creator: Tool: minimal-sbom-script
EOF
  echo "SBOM -> $OUT_DIR/sbom.spdx"
fi
