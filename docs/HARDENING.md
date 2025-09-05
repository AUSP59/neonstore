# Build Hardening

We enable modern hardening flags by default in Release (RELRO/now/noexecstack, stack protector, CET/CF on MSVC). See `ENABLE_HARDENING` and `ENABLE_LTO`. Use sanitizers in Debug (ASan/UBSan, optional MSan/TSan).