# Release Signing (Sigstore/Cosign)

We recommend signing release artifacts with Cosign and attaching SLSA provenance. The GitHub workflow `slsa-provenance.yml` is provided; configure secrets `COSIGN_KEY` to enable signing.
