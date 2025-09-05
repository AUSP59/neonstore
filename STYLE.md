# C++ Style Guide

- C++20, `.hpp` headers, `.cpp` sources.
- Headers are self-contained; include what you use.
- RAII and `noexcept` where appropriate; avoid raw `new/delete`.
- `snake_case` for files, `CamelCase` types, `snake_case` functions and variables.
- Prefer `std::string_view`, `std::span` for non-owning views.
- Avoid implicit conversions; mark single-arg constructors `explicit`.
- Error handling: throw `std::invalid_argument` / `std::runtime_error` with clear messages.
- Logging: use `std::cerr`/`std::cout` only in CLI; library stays I/O-light.
- Testing: minimal assertions in `tests/`, run via CTest.
