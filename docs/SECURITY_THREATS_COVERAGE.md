# Security Threat Coverage

- Memory safety: Address/Undefined Sanitizers; optional MSan/TSan; fuzzers for CSV inputs.
- Supply chain: CodeQL, SBOM (CycloneDX), SLSA provenance, Scorecard workflow.
- Hardening: RELRO/now/noexecstack, stack protector, CET/CF on MSVC.
- Privacy: No telemetry or network calls.
- Reproducible builds and release signing guidelines included.
