# Changelog

All notable changes to **IFM-CostIntel** are documented in this file.

## [1.0.0] - 2026-08-14

### Added
- **Foundation**: Financial Arithmetic Kernel (FAK) with strict fixed-point micro-units (`ifm_micros_t`) and compiler-checked overflow protection.
- **Foundation**: Contiguous Memory Arena (`ifm_arena_t`) and structured diagnostic engine.
- **System 1 (Ingress)**: Stream adapter, zero-allocation JSON decoder, schema validator, and lineage traceability mapper.
- **System 2 (Intelligence)**: Rule engine with priority hierarchy and ambiguity detection, cost allocation engine, multi-dimensional hash aggregation, signed baseline variance analysis, magnitude concentration engine, and explainable anomaly detection.
- **System 3 (Governance)**: Population and financial conservation reconciliation tracker, Dead Letter Queue (DLQ) engine with severity levels (`SEV_WARN`, `SEV_ERR`, `SEV_FATAL`), high-resolution monotonic telemetry, and canonical NDJSON output emitter.
- **Oracle & Verification**: Independent Python 3 reference oracle (`oracle/costintel_oracle.py`) and high-volume differential test harness (`tests/differential_test.py`).
- **Performance**: High-throughput benchmark suite achieving >2.1M records/sec.
- **Documentation**: Architecture specifications and Architectural Decision Records (ADRs).
