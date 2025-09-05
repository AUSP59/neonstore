# SECURITY.md
> Responsible Disclosure & Security Policy for **NeonStore**

**Please do not open public issues for security reports.**  
Report privately to **@AUSP59** — <alanursapu@gmail.com>. We will acknowledge within **72 hours**.

---

## How to Report a Vulnerability
Email **alanursapu@gmail.com** with subject **[SECURITY] <brief title>** and include:

- Affected component/version (commit/tag, OS, compiler/toolchain).
- Vulnerability class (e.g., OOB read, SQLi, TOCTOU) and **impact** (RCE, info leak, DOS…).
- **Reproduction steps / PoC** (CLI args, dataset, config).  
- Suggested **CVSS v3.1 vector** (optional) and any mitigations/workarounds.
- Your contact + disclosure preferences (credit name/handle).

> If you prefer encryption, mention it in your first email and we’ll provide options.

### Example Report Template
Title: OOB read in CSV parser leads to crash
Version: v1.2.0 (commit abcdef), Linux x86_64 GCC 13
Impact: Denial of service (crash)
PoC: neonstore csv import --dir ./poc
CVSS: AV:N/AC:L/PR:N/UI:N/S:U/C:N/I:N/A:H (7.5)
Notes: triggered with malformed field; root cause in csv_reader.cpp:123

yaml
Copiar código

---

## Our Process & Timelines (SLA)
1. **Acknowledgement:** within **3 business days**.
2. **Triage & severity:** within **10 business days** (CVSS-based).
3. **Fix target:** within **90 days** (expedited if exploited in the wild).
4. **Coordinated disclosure:** we’ll publish a **GitHub Security Advisory**, release patched versions, and (where applicable) request a **CVE ID**. We credit reporters who opt in.

---

## Scope
**In scope**
- Core C++ library, stable C API, CLI.
- Storage backends (CSV, optional SQLite).
- Build/packaging (CMake, CPack), Docker images, release artifacts.
- CI/CD workflows and security tooling configuration.

**Out of scope**
- Social engineering, physical attacks, third-party hosting/platform issues.
- Volumetric DDoS, rate-limit exhaustion against shared infra.
- Vulnerabilities **only** present in unmodified third-party deps (please report upstream; we will track and update).

---

## Supported Versions (Fix/Backport Policy)
| Track | Status | Security Fixes |
| --- | --- | --- |
| **Main** (next minor) | Active | Yes |
| **Latest stable** (vX.Y) | Active | Yes |
| **Previous stable** (≤ 12 months) | Maintained | Critical only |
| Older releases | EOL | No |

We follow **SemVer**; ABI breaks land in major releases. See `docs/ABI_STABILITY.md`.

---

## Vulnerability Classes We Prioritize
- Memory safety (UAF, OOB, double free), race conditions, UB.
- Injection/unsafe deserialization in inputs (CSV/SQLite paths).
- Privilege escalation in CLI or installers.
- Supply-chain risks (tampered artifacts, SBOM mismatches).

---

## Defense-in-Depth Already in Place
- **Fuzzing** (CIFuzz, seed corpus) and **sanitizers** (ASan/UBSan/TSan), **Valgrind**.
- **Static analysis**: CodeQL, Semgrep, cppcheck, DevSkim.
- **Secrets scanning**: Gitleaks.
- **Container scanning**: Trivy.
- **Supply-chain**: SBOM (CycloneDX), REUSE/SPDX, Scorecard, SLSA-ready.
- **Reproducible builds**; optional **cosign** signing and attestations.

---

## Verifying Releases
- **SBOM**: attached to each GitHub Release (`sbom.cdx.json`).
- **Checksums**: `SHA256SUMS.txt` per release.
- **Signatures (optional)**: cosign keyless (`*.sig`, `*.pem`).  
  Verify: `cosign verify-blob --certificate <asset>.pem --signature <asset>.sig <asset>`

---

## Reporting Leaked Secrets
If you discover a secret in the repo (or logs/artifacts):
1. Email **alanursapu@gmail.com** immediately with the file path/commit.
2. Do **not** share it publicly or reuse it.
3. We will rotate/revoke and purge history where appropriate.

---

## Safe Harbor
We support good-faith security research:
- Do not exfiltrate data or degrade service.
- Do not violate privacy or applicable laws.
- Make a reasonable effort to avoid privacy impacts and data destruction.
- Give us reasonable time to remediate before public disclosure.

No bug bounty is currently offered; responsible disclosures receive public credit in advisories and release notes (if desired).

---

## Contact
- **Security contact:** **@AUSP59** — <alanursapu@gmail.com>  
- We track advisories via **GitHub Security Advisories** on the repository.

_Last updated: 2025-09-04_