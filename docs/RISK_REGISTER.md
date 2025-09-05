# Risk Register (excerpt)
- Data corruption on crash during save → mitigated by atomic temp+rename writes.
- CSV injection (formula) → users must treat CSV in untrusted spreadsheet contexts; we escape quotes and commas.
- Supply-chain risk → SBOM provided; CI runs CodeQL/cppcheck/clang-tidy.
