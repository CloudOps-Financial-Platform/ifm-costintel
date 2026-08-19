# Changelog

All notable changes to **IFM-CostIntel** are documented in this file.

## [1.0.0] - 2026-08-19

### Added
- **Foundation**: Financial Arithmetic Kernel (FAK) with strict fixed-point micro-units (`ifm_micros_t`) and compiler-checked overflow protection.
- **Foundation**: Contiguous Memory Arena (`ifm_arena_t`) and structured diagnostic engine.
- **System 1 (Ingress)**: High-performance stream adapter, zero-heap JSON decoder with strict numeric validation, schema validator, and lineage traceability mapper.
- **System 2 (Intelligence)**: Rule engine with priority hierarchy and ambiguity detection, cost allocation engine, multi-dimensional hash aggregation, magnitude concentration engine, and explainable anomaly detection.
- **System 2 (Intelligence / Performance)**: $O(1)$ open-addressing baseline hash index using 64-bit FNV-1a hashing, linear probing, 70% max load factor, and fail-closed dynamic resizing.
- **System 3 (Governance)**: Population and financial conservation reconciliation tracker, Dead Letter Queue (DLQ) engine with severity levels (`SEV_WARN`, `SEV_ERR`, `SEV_FATAL`), high-resolution monotonic telemetry, and canonical NDJSON output emitter.
- **Oracle & Verification**: Independent Python 3 reference oracle (`oracle/costintel_oracle.py`) and high-volume differential test harness (`tests/differential_test.py`).
- **Fuzzing & Hardening**: 100,000-iteration adversarial fuzzer with truncation, numeric boundary, delimiter, and NUL injection testing (`tests/fuzz/fuzz_json_decoder.c`).
- **Benchmarking & Profiling**: Full 9-stage production benchmark (`ifm_costintel_bench`) measuring 1.47M records/sec (265.67 MB/sec) on 500k records, and empirical scalability profiler (`ifm_costintel_scale`) characterizing baseline performance from 100 to 100,000 items.
- **Documentation**: Comprehensive architecture specifications, ADRs, and release packaging.
