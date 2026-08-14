# IFM-CostIntel v1.0.0

**Production-grade deterministic FinOps intelligence engine written in C11.**

Consumes normalized Intermediate Financial Model (IFM) billing streams and produces deterministic cost allocations, multi-dimensional aggregations, baseline variance analysis, concentration metrics, explainable anomaly detection, and population/financial conservation reconciliation.

---

## Key Capabilities

- **Zero Floating-Point Financial Arithmetic (FAK)**: 64-bit micro-currency fixed point ($1.00 = 1,000,000 micros) with checked overflow protection.
- **Ultra-High Throughput**: Measured at **>2,100,000 records/sec** (400+ MB/sec).
- **Mathematical Invariant Reconciliation**: Proves population and financial conservation across all terminal states before releasing batch summaries.
- **Independent Python 3 Oracle**: Mathematical reference implementation with differential testing harnesses.
- **Complete Lineage & DLQ**: Preserves source line numbers, provider row IDs, and isolates malformed records into Dead Letter Queues without crashing.

---

## Build & Test

### Prerequisites
- GCC / Clang with C11 support
- CMake 3.16+
- Python 3.11+ (for reference oracle and differential test suite)

### Build
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Run Test Suite
```bash
ctest --test-dir build --output-on-failure
```

### Run Sanitizer Build (ASan + UBSan)
```bash
cmake -B build-san -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=ON
cmake --build build-san
ctest --test-dir build-san --output-on-failure
```

### Run Differential Oracle Verification
```bash
python3 tests/differential_test.py build/ifm-costintel 10000
```

### Run Benchmark Suite
```bash
./build/ifm_costintel_bench 1000000
```

---

## Usage & CLI

```bash
# Pipe streaming NDJSON through IFM-CostIntel
cat billing_stream.ndjson | ./build/ifm-costintel --config rules.json --output intelligence.ndjson --dlq dlq.ndjson --summary audit.json

# File-based execution
./build/ifm-costintel --config rules.json --input input.ndjson --output output.ndjson --dlq dlq.ndjson --summary summary.json
```

---

## License
MIT License. Copyright (c) 2026 IFM-CostIntel Authors.
