# NeonStore
> World-class C++20 inventory & retail engine — fast, safe, cross-platform — with a stable C API and a polished CLI.

[![CI](https://github.com/AUSP59/neonstore/actions/workflows/ci.yml/badge.svg)](https://github.com/AUSP59/neonstore/actions/workflows/ci.yml)
[![CodeQL](https://github.com/AUSP59/neonstore/actions/workflows/codeql.yml/badge.svg)](https://github.com/AUSP59/neonstore/actions/workflows/codeql.yml)
[![REUSE compliant](https://img.shields.io/badge/REUSE-compliant-brightgreen)](https://reuse.software/)
[![License: Apache-2.0](https://img.shields.io/badge/License-Apache--2.0-blue.svg)](./LICENSE)

NeonStore provides a production-grade **C++ library**, a **stable C API**, and a **command-line interface** for managing products, orders and analytics with **CSV/SQLite** backends. The project is engineered for **reliability, security, and supply-chain integrity** (SBOM, SLSA-ready, CodeQL/Semgrep/Trivy, signed releases), with **reproducible builds**, **ABI control**, and **omni-channel packaging**.

---

## Table of Contents
- [Features](#features)
- [Quick Start](#quick-start)
- [Build & Install](#build--install)
- [CLI Usage](#cli-usage)
- [Library Usage (C++)](#library-usage-c)
- [C API Usage (C)](#c-api-usage-c)
- [Quality Gates & Testing](#quality-gates--testing)
- [Reproducible Builds](#reproducible-builds)
- [ABI & Versioning](#abi--versioning)
- [Packaging & Releases](#packaging--releases)
- [Docs, Manpage & Completions](#docs-manpage--completions)
- [Contributing & Governance](#contributing--governance)
- [Security](#security)
- [License](#license)
- [Contact](#contact)

---

## Features
- **Modern C++20** library with hidden visibility & `NEONSTORE_API` export control.
- **Stable C API** for broad language bindings.
- **CSV** (always) and **SQLite** (optional) storage backends.
- **Fast CLI** for everyday workflows (import/export, queries, analytics).
- **Cross-platform**: Linux, macOS, Windows; x86_64/arm64.
- **First-class quality**: unit/property/E2E tests, fuzzing, sanitizers (ASan/UBSan/TSan), Valgrind.
- **Gates**: coverage, performance budgets, binary size.
- **Supply-chain**: SBOM (CycloneDX), REUSE/SPDX, CodeQL, Semgrep, Trivy, Gitleaks, DevSkim, Scorecard, SLSA-ready.
- **Reproducible builds** + optional **cosign** keyless signing.
- **Packaging**: CPack (TGZ/ZIP/DEB/RPM/NSIS) + templates for Homebrew/Winget/Chocolatey/Snap/Flatpak, vcpkg & Conan.
- **Developer Experience**: pre-commit, CMakePresets, toolchains, Docker distroless, shell completions, man page.

---

## Quick Start
```bash
# Clone
git clone https://github.com/AUSP59/neonstore.git
cd neonstore

# Release build (CSV backend, enable SQLite if you want)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DENABLE_SQLITE=ON
cmake --build build --parallel

# Run tests
ctest --test-dir build --output-on-failure

# Use the CLI
$(find build -type f -perm -111 -name neonstore -print -quit) --help
Docker (distroless, non-root)

bash
Copiar código
docker build -t neonstore:latest .
docker run --rm neonstore:latest --help
Build & Install
bash
Copiar código
# Configure (typical)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DENABLE_SQLITE=ON

# Optional hardening & dev flags
# -DENABLE_SANITIZERS=ON  -DENABLE_TSAN=ON
# -DBUILD_COVERAGE=ON     -DBUILD_FUZZING=ON
# -DENABLE_PIE=ON (Linux) -DENABLE_GLIBCXX_ASSERTIONS=ON (GCC)
cmake --build build --parallel

# Install (may need sudo on Linux/macOS)
cmake --install build
CPack packages (Release)

bash
Copiar código
cmake --build build --target package
ls build/*.{tar.gz,zip,deb,rpm,exe} 2>/dev/null || true
CLI Usage
bash
Copiar código
neonstore [global options] <command> [args]

Global options:
  --dir <path>        Data directory (CSV) or DB path (SQLite)
  --output <format>   text | json
  --limit <N>         Limit results
  --where <expr>      Filter predicate (field==value, etc.)
  --help              Show help

Commands:
  csv import|export   Import/export CSV datasets
  query               Query products/orders
  analytics           Basic KPIs (stock value, top sellers)
  doctor              Validate dataset & print diagnostics
Examples:

bash
Copiar código
# Import products from CSV
neonstore csv import --dir ./data

# List products as JSON
neonstore query products --output json --limit 20

# Analytics snapshot
neonstore analytics --dir ./data
Library Usage (C++)
cpp
Copiar código
#include <neonstore/inventory.hpp>
#include <iostream>
int main() {
  neonstore::Inventory inv;
  neonstore::Product p{ "P1", "Widget", 9.99, 5 };
  inv.add_product(p);
  for (const auto& it : inv.list_products())
    std::cout << it.id << " " << it.name << "\n";
  return 0;
}
Linking
CMake:

cmake
Copiar código
find_package(NeonStoreSystem CONFIG REQUIRED)
add_executable(app main.cpp)
target_link_libraries(app PRIVATE NeonStoreSystem::neonstore_lib)
C API Usage (C)
c
Copiar código
#include "neonstore/capi.h"
#include <stdio.h>
int main() {
  void* h = NULL;
  if (ns_inventory_create(&h) != 0) return 1;
  ns_product_t p = { "P1", "Widget", 9.99, 5 };
  ns_inventory_add_product(h, p);
  int n = ns_inventory_list_products(h, NULL, 0);
  printf("Products: %d\n", n);
  ns_inventory_destroy(h);
  return 0;
}
Compile:

bash
Copiar código
cc -Iinclude -Lbuild -lneonstoresystem examples/c_api_example.c -o c_demo
./c_demo
Quality Gates & Testing
bash
Copiar código
# Unit, property & E2E tests
ctest --test-dir build --output-on-failure

# Coverage (threshold gate >= 60% by default)
cmake -S . -B build -DBUILD_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build && ctest --test-dir build
python3 tools/coverage_gate.py build/coverage.xml 0.60

# Fuzzing (60s sanity)
cmake -S . -B build -DBUILD_FUZZING=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build && timeout 60 ./build/fuzz/fuzz_csv_products || true

# Performance & size budgets
python3 tools/perf_gate.py 3000 3000
python3 tools/size_gate.py $(find build -type f -perm -111 -name neonstore -print -quit) 15000000
Reproducible Builds
bash
Copiar código
export SOURCE_DATE_EPOCH=1609459200
cmake -S . -B b1 -DCMAKE_BUILD_TYPE=Release && cmake --build b1 -j
cmake -S . -B b2 -DCMAKE_BUILD_TYPE=Release && cmake --build b2 -j
sha256sum b1/…/neonstore b2/…/neonstore   # should match
See: docs/REPRODUCIBLE_BUILDS.md.

ABI & Versioning
SemVer for the C++ library and C API.

Hidden visibility by default; exported symbols via NEONSTORE_API.

Shared libs ship with SONAME; ABI monitored in CI.
See: docs/ABI_STABILITY.md, docs/ABI_COMPLIANCE_CI.md, and include/neonstore/version.hpp.

Packaging & Releases
CPack artifacts for Linux/macOS/Windows.

Templates for Homebrew, Winget, Chocolatey, Debian, RPM, Snap, Flatpak in packaging/.

vcpkg port under ports/neonstoresystem/, Conan test_package/.

Release playbook: docs/RELEASE_PLAYBOOK.md.

SBOM (CycloneDX) is generated and attached in releases.

Optional cosign keyless signing workflows are included.

Docs, Manpage & Completions
API & ops docs live in docs/.

Manpage: docs/man/neonstore.1 (generated in CI via help2man).

Shell completions in completions/ (bash, zsh, fish) — installed by CMake on UNIX.

Contributing & Governance
Please read CONTRIBUTING.md and follow CODE_OF_CONDUCT.md.

DCO sign-off and Conventional Commits required.

Issue/PR templates and labels help us triage quickly.

Security
Report vulnerabilities privately per SECURITY.md.
Do not file public issues for security reports.

Process & SLAs: docs/SECURITY_RESPONSE.md.

Supply-chain practices: SBOM (CycloneDX), REUSE/SPDX, CodeQL/Semgrep/Gitleaks/DevSkim, Trivy, Scorecard, SLSA-ready.

License
This project is licensed under Apache-2.0. See LICENSE.
All files include SPDX identifiers and follow REUSE best practices.

Contact
Maintainer of record: @AUSP59 — alanursapu@gmail.com
Issues & discussions welcome in the GitHub repository.

“Build once, trust everywhere.”