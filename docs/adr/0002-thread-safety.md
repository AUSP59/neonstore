# ADR 0002: Per-instance Thread Safety
- Status: Accepted
- Date: 2025-09-04
- Decision: guard mutations with a per-instance `std::mutex` when `ENABLE_THREADSAFE` is ON.
- Consequence: safer concurrent usage; negligible overhead for single-threaded usage.
