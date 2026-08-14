# ADR 0002: Zero-Allocation Ingress Pipeline & Memory Arena

## Context
High-throughput stream processing (>250k records/sec) can suffer severe memory fragmentation and latency spikes if every JSON record triggers individual heap allocations (`malloc` and `free`).

## Decision
- Built a zero-allocation JSON tokenizer and decoder that works directly on stream buffer slices.
- Implemented a contiguous memory arena (`ifm_arena_t`) for batch lifetimes and dynamic structures.

## Status
Accepted and Implemented. (Achieved >2.1M records/sec).
