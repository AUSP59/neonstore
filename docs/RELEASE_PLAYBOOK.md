# Release Playbook
1. Ensure CHANGELOG/Release Drafter is up-to-date.
2. Tag `vX.Y.Z` and push tags.
3. `release-build.yml` attaches CPack artifacts to Release.
4. Run `cosign-release.yml` (automatic on publish) to sign all assets.
5. Upload `abi/current.abi` as baseline if bumping minor with ABI stability.
6. Publish to registries using templates in `packaging/`.
