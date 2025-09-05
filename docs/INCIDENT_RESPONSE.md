# Incident Response

1. Triage and confirm impact.
2. Assign CVE if applicable (via GitHub Advisory or CNA).
3. Prepare patch in private fork; run CI + CodeQL.
4. Publish advisory with affected versions and mitigation steps.
5. Cut release and backports; notify downstreams.
