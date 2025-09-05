# Release

1. Ensure CI is green on `main`.
2. Bump version in headers and `CMakeLists.txt`.
3. Update `CHANGELOG.md`.
4. Produce archives and **SBOM** (`make sbom`).
5. Create a signed tag and a GitHub Release.
