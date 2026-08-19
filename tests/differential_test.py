#!/usr/bin/env python3
"""
High-Volume Differential Testing Suite:
Generates 10,000+ multi-cloud billing records spanning edge cases,
monetary boundaries, priority hierarchies, ambiguous rule conflicts,
and verifies 100% agreement between the C11 engine and the Python 3 Reference Oracle.
"""

import subprocess
import json
import tempfile
import random
import os
import sys

def generate_random_record(idx: int) -> dict:
    providers = ["aws", "azure", "gcp", "custom"]
    provider = random.choice(providers)
    
    if provider == "aws":
        account_id = f"111{random.randint(100, 999)}" if random.random() < 0.5 else f"999{random.randint(100, 999)}"
        resource_id = f"i-{random.randint(1000, 9999)}" if random.random() < 0.6 else f"vol-{random.randint(1000, 9999)}"
    elif provider == "azure":
        account_id = f"sub-{random.randint(1, 10)}"
        resource_id = f"vm-{random.randint(100, 999)}"
    elif provider == "gcp":
        account_id = f"proj-{random.randint(1, 10)}"
        resource_id = f"gcs-{random.randint(100, 999)}"
    else:
        account_id = f"onprem-{random.randint(1, 5)}"
        resource_id = f"rack-{random.randint(10, 99)}"

    # Generate varied costs
    cost_type = random.choice(["micros", "decimal", "zero", "negative"])
    if cost_type == "micros":
        cost_micros = random.randint(1, 100_000_000)
        return {
            "source_line": idx,
            "provider": provider,
            "provider_row_id": f"row-{idx:06d}",
            "account_id": account_id,
            "resource_id": resource_id,
            "usage_start_raw": "2026-08-14T00:00:00Z",
            "billed_cost_micros": cost_micros
        }
    elif cost_type == "decimal":
        dollars = random.randint(1, 500)
        cents = random.randint(0, 999999)
        return {
            "source_line": idx,
            "provider": provider,
            "provider_row_id": f"row-{idx:06d}",
            "account_id": account_id,
            "resource_id": resource_id,
            "usage_start_raw": "2026-08-14T00:00:00Z",
            "billed_cost": f"{dollars}.{cents:06d}"
        }
    elif cost_type == "zero":
        return {
            "source_line": idx,
            "provider": provider,
            "provider_row_id": f"row-{idx:06d}",
            "account_id": account_id,
            "resource_id": resource_id,
            "usage_start_raw": "2026-08-14T00:00:00Z",
            "billed_cost_micros": 0
        }
    else: # negative credit
        cost_micros = -random.randint(1, 10_000_000)
        return {
            "source_line": idx,
            "provider": provider,
            "provider_row_id": f"row-{idx:06d}",
            "account_id": account_id,
            "resource_id": resource_id,
            "usage_start_raw": "2026-08-14T00:00:00Z",
            "billed_cost_micros": cost_micros
        }

def run_large_differential_test(c_bin_path: str, count: int = 10000):
    print(f"===============================================================")
    print(f"Executing Large-Scale Differential Test ({count:,} records)")
    print(f"Binary: {c_bin_path}")
    print(f"===============================================================")

    random.seed(42)
    records = [generate_random_record(i + 1) for i in range(count)]

    rules_config = {
        "config_version": "v1.0.0",
        "rules": [
            {"rule_id": "RULE-AWS-COMPUTE", "priority": 100, "match_provider": "aws", "match_account_id": "111*", "match_resource_prefix": "i-", "target_cost_center_id": "CC-AWS-COMPUTE", "version": 1},
            {"rule_id": "RULE-AWS-STORAGE", "priority": 90, "match_provider": "aws", "match_account_id": "111*", "match_resource_prefix": "vol-", "target_cost_center_id": "CC-AWS-STORAGE", "version": 1},
            {"rule_id": "RULE-AWS-OVERRIDE", "priority": 200, "match_provider": "aws", "match_account_id": "999*", "target_cost_center_id": "CC-VIP", "version": 2},
            {"rule_id": "RULE-AZURE", "priority": 50, "match_provider": "azure", "target_cost_center_id": "CC-AZURE", "version": 1},
            {"rule_id": "RULE-GCP", "priority": 50, "match_provider": "gcp", "target_cost_center_id": "CC-GCP", "version": 1},
            {"rule_id": "RULE-CONFLICT-1", "priority": 75, "match_provider": "custom", "target_cost_center_id": "CC-CUSTOM-A", "version": 1},
            {"rule_id": "RULE-CONFLICT-2", "priority": 75, "match_provider": "custom", "target_cost_center_id": "CC-CUSTOM-B", "version": 1}
        ],
        "baselines": [
            {"key": "i-1000", "baseline_micros": 50000000},
            {"key": "i-2000", "baseline_micros": 10000000},
            {"key": "vm-100", "baseline_micros": 20000000},
            {"key": "gcs-100", "baseline_micros": 5000000}
        ],
        "anomalies": [
            {"rule_id": "ANOMALY-SPIKE-50", "threshold_pct_micros": 500000, "min_baseline_micros": 5000000, "direction": "SPIKE"},
            {"rule_id": "ANOMALY-DROP-30", "threshold_pct_micros": 300000, "min_baseline_micros": 5000000, "direction": "DROP"}
        ]
    }

    with tempfile.TemporaryDirectory() as tmpdir:
        in_path = os.path.join(tmpdir, "input.ndjson")
        cfg_path = os.path.join(tmpdir, "config.json")
        c_out_path = os.path.join(tmpdir, "c_out.ndjson")
        py_out_path = os.path.join(tmpdir, "py_out.ndjson")

        with open(in_path, "w") as f:
            for r in records:
                f.write(json.dumps(r) + "\n")

        with open(cfg_path, "w") as f:
            json.dump(rules_config, f)

        # Run C binary
        cmd_c = [c_bin_path, "--config", cfg_path, "--input", in_path, "--output", c_out_path]
        res_c = subprocess.run(cmd_c, capture_output=True, text=True)
        if res_c.returncode != 0:
            print("C binary failed:", res_c.stderr)
            sys.exit(1)

        # Run Python Oracle
        oracle_path = os.path.join(os.path.dirname(__file__), "../oracle/costintel_oracle.py")
        cmd_py = [sys.executable, oracle_path, "--config", cfg_path, "--input", in_path, "--output", py_out_path]
        res_py = subprocess.run(cmd_py, capture_output=True, text=True)
        if res_py.returncode != 0:
            print("Python oracle failed:", res_py.stderr)
            sys.exit(1)

        # Verification
        mismatches = 0
        with open(c_out_path) as fc, open(py_out_path) as fp:
            line_idx = 0
            for line_c, line_py in zip(fc, fp):
                line_idx += 1
                rec_c = json.loads(line_c)
                rec_py = json.loads(line_py)

                for k in ["provider", "account_id", "resource_id", "allocation_status", "cost_center_id", "rule_id", "active_spend_micros", "baseline_micros", "variance_delta_micros", "variance_status", "is_anomaly", "anomaly_rule_id", "anomaly_direction"]:
                    if rec_c.get(k) != rec_py.get(k):
                        print(f"Mismatch at line {line_idx} on '{k}': C={rec_c.get(k)} vs Py={rec_py.get(k)}")
                        mismatches += 1
                        if mismatches >= 5:
                            sys.exit(1)

        assert mismatches == 0
        print(f"DIFFERENTIAL TEST PASSED: {line_idx:,} records verified with 100% agreement!")

if __name__ == "__main__":
    bin_path = sys.argv[1] if len(sys.argv) > 1 else "/home/mrcn2/ifm-costintel/build/ifm-costintel"
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 10000
    run_large_differential_test(bin_path, count)
