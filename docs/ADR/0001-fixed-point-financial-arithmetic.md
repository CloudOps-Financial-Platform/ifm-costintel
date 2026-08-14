# ADR 0001: Strict Fixed-Point Financial Arithmetic Kernel (FAK)

## Context
Standard floating-point representations (`float`, `double`, IEEE 754) introduce binary rounding errors (e.g. `0.1 + 0.2 != 0.3`) and platform-dependent floating-point rounding modes that violate financial determinism and auditability.

## Decision
All currency and monetary calculations in IFM-CostIntel strictly use 64-bit signed integers (`ifm_micros_t`) representing micro-units ($1.00 = 1,000,000 micros).
- Floating-point types are prohibited in all financial calculation paths.
- All operations are verified using checked compiler intrinsics (`__builtin_add_overflow`, `__builtin_sub_overflow`, `__builtin_mul_overflow`).
- Intermediate multiplication scales through 128-bit integers (`ifm_int128_t`).

## Status
Accepted and Implemented.
