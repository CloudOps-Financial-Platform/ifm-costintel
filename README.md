# IFM-CostIntel v1.0.0

**Production-grade deterministic FinOps intelligence engine written in C11.**

`IFM-CostIntel` (Product #2 of the CloudOps Financial Platform) consumes normalized Intermediate Financial Model (IFM) billing records in NDJSON format and executes high-performance cost allocation, 4-dimensional aggregation, $O(1)$ baseline variance analysis, magnitude concentration, explainable anomaly detection, and population/financial conservation reconciliation.

---

## Architectural Position & Scope

```
┌─────────────────────────────────────────────────────────────┐
│ Upstream Cloud Ingestion & Normalization                    │
│ Product #1: billing-data-gateway (Separate Future Product)  │
└──────────────────────────────┬──────────────────────────────┘
                               │
                               │ IFM Normalized NDJSON Stream
                               ▼
┌─────────────────────────────────────────────────────────────┐
│ Product #2: IFM-CostIntel (Completed Engine v1.0.0)         │
│                                                             │
│  [1. Ingress] ──► [2. JSON Decode] ──► [3. Traceability]   │
│         │                                                   │
│         ▼                                                   │
│  [4. Schema Validate] ──► [5. Allocation] ──► [6. 4D Agg]  │
│         │                                                   │
│         ▼                                                   │
│  [7. O(1) Baseline] ──► [8. Variance/Conc] ──► [9. Anomaly]│
│         │                                                   │
│         ▼                                                   │
│  [10. Reconciliation Invariant Check] ──► [11. Output/DLQ] │
└──────────────────────────────┬──────────────────────────────┘
                               │
                               ▼
                  Canonical FinOps Intelligence
```

> **Note on Platform Architecture**: `IFM-CostIntel` is **Product #2**. Upstream ingestion from raw cloud provider billing APIs is designated for **Product #1 (`billing-data-gateway`)**, which is a separate future product. Product #2 contract verification confirms full compatibility with the standard IFM NDJSON schema specification.

---

## 9-Stage Financial Pipeline Architecture

1. **Ingress Streaming (`stream_adapter.c`)**: Buffered high-throughput chunk reader for stdin, pipes, or files.
2. **Zero-Heap JSON Decoding (`json_decoder.c`)**: High-performance tokenizer parsing IFM records with strict numeric validation and zero per-record heap allocation.
3. **Traceability Mapping (`traceability.c`)**: Attaches source line numbers and provider row identifiers to every record.
4. **Schema Validation (`schema_validator.c`)**: Enforces required multi-cloud fields (`provider`, `account_id`, `resource_id`, `billed_cost_micros`).
5. **Rule Allocation (`rules.c`, `allocation.c`)**: Hierarchical pattern matching across dimensions, classifying records as `ALLOCATED`, `UNALLOCATED`, or `AMBIGUOUS`.
6. **4D Aggregation (`aggregation.c`)**: Arena-backed multi-dimensional hash tables grouping by Provider, Account, Cost Center, and Resource with checked overflow protection.
7. **O(1) Baseline Lookup (`variance.c`)**: Open-addressing hash table with 64-bit FNV-1a hashing and linear probing, ensuring sub-25ns lookups at 100,000+ baseline entries.
8. **Signed Variance & Concentration (`variance.c`, `concentration.c`)**: Exact fixed-point delta calculation ($\text{Active} - \text{Baseline}$) and relative magnitude assessment ($\text{Spend} / \text{Grand Total}$).
9. **Explainable Anomaly Detection (`anomaly.c`)**: Configurable percentage spike/drop detection with explicit rule attribution.
10. **Conservation Reconciliation (`reconciliation.c`)**: Enforces population ($\sum \text{Count} = N$) and financial ($\sum \text{Micros} = \text{Total}$) conservation invariants across all terminal states before releasing results.
11. **Telemetry & Output (`telemetry.c`, `output.c`, `fault_engine.c`)**: Monotonic timing, audit summary emission, canonical NDJSON intelligence formatting, and Dead Letter Queue (DLQ) routing.

---

## Core Engineering Features

- **Financial Arithmetic Kernel (FAK)**: 64-bit micro-currency fixed point ($1.00 = 1,000,000 micros) utilizing compiler-checked arithmetic overflow intrinsics. Zero floating-point types in financial paths.
- **O(1) Baseline Hash Index**: Open-addressing hash table with FNV-1a 64-bit hashing, 70% load factor threshold, and automatic power-of-two rehash. Sustains >1.04M–1.70M rec/s across all lookup patterns at 100,000 baseline items.
- **Fail-Closed Configuration Ingress**: Strict validation of rule, baseline, and anomaly configurations; any parse or validation failure cleanly resets state and aborts.
- **Adversarial Ingress Fuzzing**: 100,000-iteration deterministic adversarial fuzzer testing truncated JSON, boundary overflows, NUL injections, and delimiter corruption.
- **Reference Oracle & Differential Testing**: Python 3 mathematical oracle (`oracle/costintel_oracle.py`) verified with 100% agreement over 10,000-record differential runs.

---

## Empirical Benchmark & Scalability Metrics

### Full 9-Stage Pipeline Benchmark (`ifm_costintel_bench`)
- **Workload**: 500,000 records, rotating 16-record multi-cloud corpus (AWS, Azure, GCP), 4 arena-backed aggregation dimensions.
- **Pipeline Throughput**: **1,470,516 records/sec**
- **Data Bandwidth**: **265.67 MB/sec**
- **Conservation Invariant**: **PASS (100% Mathematically Conserved)**

### Baseline Scalability Curve (`ifm_costintel_scale`)
Measured across 15 permutations ($N \in \{100, 1000, 10000, 50000, 100000\}$ across Uniform Hit, Tail Hit, and Miss):

| Baseline Entries ($N$) | Lookup Mode | Isolated Lookup Latency | Pipeline Throughput | Reconciliation Check |
|:-----------------------|:------------|:------------------------|:--------------------|:---------------------|
| **100**                | Uniform Hit | 24.54 ns                | 1,186,571 rec/s     | PASS (100%)          |
|                        | Tail Hit    | 15.00 ns                | 1,743,094 rec/s     | PASS (100%)          |
|                        | Miss        | 12.26 ns                | 1,744,786 rec/s     | PASS (100%)          |
| **1,000**              | Uniform Hit | 13.21 ns                | 1,361,014 rec/s     | PASS (100%)          |
|                        | Tail Hit    | 11.63 ns                | 1,154,612 rec/s     | PASS (100%)          |
|                        | Miss        | 7.84 ns                 | 1,773,200 rec/s     | PASS (100%)          |
| **10,000**             | Uniform Hit | 13.18 ns                | 1,735,325 rec/s     | PASS (100%)          |
|                        | Tail Hit    | 15.73 ns                | 1,482,763 rec/s     | PASS (100%)          |
|                        | Miss        | 9.84 ns                 | 1,551,532 rec/s     | PASS (100%)          |
| **50,000**             | Uniform Hit | 11.73 ns                | 1,824,204 rec/s     | PASS (100%)          |
|                        | Tail Hit    | 10.56 ns                | 1,536,583 rec/s     | PASS (100%)          |
|                        | Miss        | 7.05 ns                 | 1,735,915 rec/s     | PASS (100%)          |
| **100,000**            | Uniform Hit | 11.51 ns                | 1,289,050 rec/s     | PASS (100%)          |
|                        | Tail Hit    | 24.35 ns                | 1,042,286 rec/s     | PASS (100%)          |
|                        | Miss        | 9.13 ns                 | 1,704,042 rec/s     | PASS (100%)          |

---

## Build, Verification & Execution

### Prerequisites
- GCC / Clang with C11 support
- CMake 3.16+
- Python 3.11+ (for reference oracle and differential testing)

### Standard Release Build & CTest
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --clean-first
ctest --test-dir build --output-on-failure
```

### Sanitizer Build (AddressSanitizer + UndefinedBehaviorSanitizer)
```bash
cmake -B build-san -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=ON
cmake --build build-san --clean-first
ctest --test-dir build-san --output-on-failure
```

### Differential Oracle Verification (10,000 records)
```bash
python3 tests/differential_test.py build/ifm-costintel 10000
```

### Production Benchmarking & Scalability Profiling
```bash
# Run 500,000 record 9-stage production benchmark
./build/ifm_costintel_bench

# Run 15-tier empirical scalability profiler (50,000 records/tier)
./build/ifm_costintel_scale 50000
```

---

## CLI Usage

```bash
# Streaming pipeline execution
cat billing_records.ndjson | ./build/ifm-costintel \
  --config config/rules.json \
  --output intelligence.ndjson \
  --dlq dlq.ndjson \
  --summary audit_summary.json

# File-based execution
./build/ifm-costintel \
  --config config/rules.json \
  --input billing_records.ndjson \
  --output intelligence.ndjson \
  --dlq dlq.ndjson \
  --summary audit_summary.json
```

---

## License

MIT License. Copyright (c) 2026 IFM-CostIntel Authors.
