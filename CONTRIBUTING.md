# CONTRIBUTING.md
Thank you for your interest in improving **NeonStore**! This guide explains how to propose changes safely and efficiently.

> By participating, you agree to follow our [Code of Conduct](./CODE_OF_CONDUCT.md).  
> Primary maintainer/contact: **@AUSP59** · <alanursapu@gmail.com>

---

## 1) Ways to Contribute
- 🐞 Report bugs and regressions
- 💡 Propose or implement features
- 🧪 Add/extend tests (unit, property, fuzz, e2e)
- 📚 Improve docs (README, man page, API refs)
- 🛡️ Security: follow [SECURITY.md](./SECURITY.md)
- 🌍 Packaging (DEB/RPM/Homebrew/Winget/Chocolatey), CI, dev tooling

Open an issue first for large changes to align on scope/design.

---

## 2) Development Setup
**Requirements**
- CMake ≥ 3.20, C++20 compiler (GCC 11+/Clang 14+/MSVC 19.3+)
- Git, Python 3.8+ (for tools), optional: SQLite dev headers
- Linux/macOS/Windows (x86_64/arm64 supported)

**Clone**
```bash
git clone https://github.com/<your-org>/neonstore.git
cd neonstore
Configure & Build (Release)

bash
Copiar código
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DENABLE_SQLITE=ON
cmake --build build --parallel
Run Tests

bash
Copiar código
ctest --test-dir build --output-on-failure
Useful Options

-DENABLE_SANITIZERS=ON (ASan/UBSan)

-DENABLE_TSAN=ON (ThreadSanitizer)

-DBUILD_COVERAGE=ON

-DBUILD_FUZZING=ON

-DENABLE_PIE=ON (Linux)

-DENABLE_GLIBCXX_ASSERTIONS=ON (GCC)

3) Pre-commit & Style
We enforce style and basic hygiene via pre-commit.

bash
Copiar código
pip install pre-commit
pre-commit install
pre-commit run -a
Hooks include: clang-format/clang-tidy, codespell, yamllint, cmake-format, markdownlint, SPDX/REUSE checks.

C++ Guidelines

Prefer RAII, value semantics, std::unique_ptr/std::shared_ptr over raw new.

Avoid exceptions for control flow; use expected-style patterns where practical.

Prefer string_view, span, and constexpr where sensible.

Keep functions small, testable; document non-obvious behavior.

All public headers use SPDX headers and remain warning-free with -Wall -Wextra -Werror under CI.

Formatting is auto-enforced; do not fight the formatter.

4) Testing & Quality Gates
Unit/Property/E2E

bash
Copiar código
ctest --test-dir build --output-on-failure
Coverage

bash
Copiar código
cmake -S . -B build -DBUILD_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build
python3 tools/coverage_gate.py build/coverage.xml 0.60
Fuzzing (libFuzzer)

bash
Copiar código
cmake -S . -B build -DBUILD_FUZZING=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build
# Run 60s sanity
timeout 60 ./build/fuzz/fuzz_csv_products || true
timeout 60 ./build/fuzz/fuzz_csv_orders   || true
Performance & Size Budgets

bash
Copiar código
python3 tools/perf_gate.py 3000 3000
python3 tools/size_gate.py ./build/neonstore 15000000
Sanitizers/Valgrind

bash
Copiar código
cmake -S . -B build -DENABLE_SANITIZERS=ON -DCMAKE_BUILD_TYPE=Debug && cmake --build build
valgrind --error-exitcode=1 $(find build -type f -perm -111 -name neonstore -print -quit) --help
5) API/ABI & Versioning
We follow SemVer for the C++ library and stable C API:

Do not break public ABI without a major bump.

Exposed symbols are controlled via NEONSTORE_API.

Update docs: docs/ABI_STABILITY.md, docs/ABI_COMPLIANCE_CI.md.

If ABI changes: refresh the ABI baseline and note in the changelog.

Version macros: neonstore/version.hpp.

6) Security & Licensing
Report vulnerabilities per SECURITY.md. Do not open public issues for 0-days.

No secrets in the repo; run secret scanners locally if possible.

All source files must include an SPDX license header; keep REUSE compliance.

7) Documentation
User docs live in README.md and docs/.

The man page is generated via help2man in CI (docs/man/neonstore.1).

Keep examples updated (examples/), including the C API example.

If you add CLI flags/features, update completions in completions/.

8) Packaging & Release Hygiene
CPack packages: TGZ/ZIP/DEB/RPM/NSIS.

When changing install paths/targets, update manifests under packaging/.

Release checklist: docs/RELEASE_PLAYBOOK.md (tags, SBOM, signing).

Ensure SBOM (CycloneDX) remains accurate.

9) Commit & PR Process
Conventional Commits (required)

pgsql
Copiar código
feat(storage): add bulk CSV import
fix(cli): handle CRLF on Windows
docs(readme): clarify usage
test(fuzz): seed corpus for CSV parser
build(ci): add macOS runner
DCO Sign-off (required)

pgsql
Copiar código
Signed-off-by: Your Name <you@example.com>
Enable automatically:

bash
Copiar código
git config --global user.name "Your Name"
git config --global user.email "you@example.com"
git commit -s -m "feat: your message"
PR Checklist

 Issue linked and scope agreed

 Tests added/updated; CI green locally (pre-commit run -a, ctest)

 Docs updated (README/man/completions/API)

 No ABI break (or documented with SemVer bump)

 SPDX/REUSE compliant

Small, focused PRs get reviewed faster. Expect polite, technical reviews.

10) Issue Triage (labels)
bug, regression, feat, help wanted, good first issue, a11y, performance, security, packaging, docs.
Provide repro steps, expected vs actual, logs, and environment.

11) Contact
Maintainer of record: @AUSP59 — alanursapu@gmail.com
We try to acknowledge reports within 3 business days.

Thanks for making NeonStore better!