#!/usr/bin/env python3
"""
IFM-CostIntel Python Reference Implementation & Mathematical Oracle.
Acts as an independent oracle implementing exact fixed-point financial math,
rule allocation, variance analysis, anomaly detection, and reconciliation invariants.
"""

import sys
import json
import argparse
from typing import Dict, Any, List, Optional, Tuple

MICROS_PER_UNIT = 1_000_000

def parse_micros(val: Any) -> int:
    if isinstance(val, int):
        return val
    if isinstance(val, str):
        val = val.strip()
        negative = False
        if val.startswith('-'):
            negative = True
            val = val[1:]
        elif val.startswith('+'):
            val = val[1:]
        parts = val.split('.')
        if len(parts) == 1:
            int_part = int(parts[0])
            frac_part = 0
        elif len(parts) == 2:
            int_part = int(parts[0])
            frac_str = (parts[1] + "000000")[:6]
            frac_part = int(frac_str)
        else:
            raise ValueError(f"Invalid decimal string: {val}")
        micros = int_part * MICROS_PER_UNIT + frac_part
        return -micros if negative else micros
    raise ValueError(f"Unsupported cost type: {type(val)}")

def match_pattern(pattern: str, value: str) -> bool:
    if not pattern or pattern == "*":
        return True
    if pattern.endswith("*"):
        return value.startswith(pattern[:-1])
    return pattern == value

def match_prefix(prefix: str, value: str) -> bool:
    if not prefix or prefix == "*":
        return True
    if prefix.endswith("*"):
        prefix = prefix[:-1]
    return value.startswith(prefix)

class CostIntelOracle:
    def __init__(self, rules_config: Optional[Dict[str, Any]] = None):
        self.rules = []
        self.baselines = {}
        self.anomalies = []

        if rules_config:
            if "rules" in rules_config:
                self.rules = rules_config["rules"]
            if "baselines" in rules_config:
                for b in rules_config["baselines"]:
                    key = b.get("key", b.get("resource_id", ""))
                    if "baseline_micros" in b:
                        self.baselines[key] = parse_micros(b["baseline_micros"])
                    elif "baseline" in b:
                        self.baselines[key] = parse_micros(b["baseline"])
                    elif "cost" in b:
                        self.baselines[key] = parse_micros(b["cost"])
            if "anomalies" in rules_config:
                self.anomalies = rules_config["anomalies"]

    def allocate_record(self, record: Dict[str, Any]) -> Tuple[str, str, str, int]:
        if record.get("is_faulted", False):
            return "FAULTED", "", "", 0

        matching_rules = []
        for r in self.rules:
            prov = r.get("match_provider", r.get("provider", "*"))
            acc = r.get("match_account_id", r.get("account_id", "*"))
            res = r.get("match_resource_prefix", r.get("resource_prefix", "*"))

            if match_pattern(prov, record.get("provider", "")) and \
               match_pattern(acc, record.get("account_id", "")) and \
               match_prefix(res, record.get("resource_id", "")):
                matching_rules.append(r)

        if not matching_rules:
            return "UNALLOCATED", "UNALLOCATED", "NONE", 0

        highest_prio = max(r.get("priority", 100) for r in matching_rules)
        best_candidates = [r for r in matching_rules if r.get("priority", 100) == highest_prio]

        # Check for ambiguity
        cost_centers = {r.get("target_cost_center_id", r.get("cost_center_id", "")) for r in best_candidates}
        if len(cost_centers) > 1:
            return "AMBIGUOUS", "AMBIGUOUS", "AMBIGUOUS", 0

        chosen = best_candidates[0]
        return "ALLOCATED", chosen.get("target_cost_center_id", chosen.get("cost_center_id", "")), chosen.get("rule_id", ""), chosen.get("version", 1)

    def evaluate_anomalies(self, record: Dict[str, Any], baseline_micros: int, variance_delta: int) -> Tuple[bool, str, str]:
        if not self.anomalies or record.get("variance_status") != "DEFINED" or baseline_micros <= 0:
            return False, "", "NONE"

        for r in self.anomalies:
            min_base = r.get("min_baseline_micros", 0)
            if isinstance(min_base, str):
                min_base = parse_micros(min_base)
            if baseline_micros < min_base:
                continue

            thresh = r.get("threshold_pct_micros", 0)
            if isinstance(thresh, str):
                thresh = parse_micros(thresh)

            direction = r.get("direction", "SPIKE").upper()
            abs_delta = abs(variance_delta)
            pct_change = (abs_delta * MICROS_PER_UNIT) // baseline_micros

            if pct_change >= thresh:
                if variance_delta > 0 and direction in ("SPIKE", "BOTH"):
                    return True, r.get("rule_id", ""), "SPIKE"
                elif variance_delta < 0 and direction in ("DROP", "BOTH"):
                    return True, r.get("rule_id", ""), "DROP"

        return False, "", "NONE"

    def process_record(self, raw_line: str, line_num: int, default_baseline: int = 0) -> Tuple[Optional[Dict[str, Any]], Optional[Dict[str, Any]]]:
        raw_line = raw_line.strip()
        if not raw_line:
            return None, None

        # JSON Decode
        try:
            data = json.loads(raw_line)
        except Exception as e:
            fault = {
                "source_line": line_num,
                "provider": "",
                "provider_row_id": "",
                "account_id": "",
                "resource_id": "",
                "billed_cost_micros": 0,
                "fault_code": "JSON_SYNTAX",
                "fault_severity": "ERROR",
                "fault_message": str(e)
            }
            return None, fault

        # Schema Validation
        provider = data.get("provider", "")
        account_id = data.get("account_id", "")
        resource_id = data.get("resource_id", "")

        if not provider or not account_id or not resource_id:
            fault = {
                "source_line": line_num,
                "provider": provider,
                "provider_row_id": data.get("provider_row_id", ""),
                "account_id": account_id,
                "resource_id": resource_id,
                "billed_cost_micros": 0,
                "fault_code": "MISSING_REQUIRED_FIELD",
                "fault_severity": "ERROR",
                "fault_message": "Missing required field"
            }
            return None, fault

        # Parse cost
        try:
            if "billed_cost_micros" in data:
                billed_cost_micros = parse_micros(data["billed_cost_micros"])
            elif "billed_cost" in data:
                billed_cost_micros = parse_micros(data["billed_cost"])
            elif "cost" in data:
                billed_cost_micros = parse_micros(data["cost"])
            else:
                billed_cost_micros = 0
        except Exception as e:
            fault = {
                "source_line": line_num,
                "provider": provider,
                "provider_row_id": data.get("provider_row_id", ""),
                "account_id": account_id,
                "resource_id": resource_id,
                "billed_cost_micros": 0,
                "fault_code": "INVALID_NUMERIC",
                "fault_severity": "ERROR",
                "fault_message": str(e)
            }
            return None, fault

        record = {
            "source_line": line_num,
            "provider": provider,
            "provider_row_id": data.get("provider_row_id", ""),
            "account_id": account_id,
            "resource_id": resource_id,
            "usage_start_raw": data.get("usage_start_raw", data.get("usage_start", "")),
            "billed_cost_micros": billed_cost_micros,
            "active_spend_micros": billed_cost_micros
        }

        # Allocation
        alloc_status, cost_center, rule_id, rule_ver = self.allocate_record(record)
        record["allocation_status"] = alloc_status
        record["cost_center_id"] = cost_center
        record["rule_id"] = rule_id
        record["rule_version"] = rule_ver

        # Baseline & Variance
        baseline_micros = self.baselines.get(resource_id, default_baseline)
        variance_delta = billed_cost_micros - baseline_micros
        record["baseline_micros"] = baseline_micros
        record["variance_delta_micros"] = variance_delta

        if baseline_micros == 0:
            if billed_cost_micros == 0:
                record["variance_status"] = "BASELINE_ZERO_NO_CHANGE"
            else:
                record["variance_status"] = "BASELINE_ZERO"
        else:
            record["variance_status"] = "DEFINED"

        # Anomaly Detection
        is_anom, anom_rule, anom_dir = self.evaluate_anomalies(record, baseline_micros, variance_delta)
        record["is_anomaly"] = is_anom
        record["anomaly_rule_id"] = anom_rule
        record["anomaly_direction"] = anom_dir

        return record, None

def main():
    parser = argparse.ArgumentParser(description="IFM-CostIntel Python Reference Oracle")
    parser.add_argument("--config", help="Config JSON file")
    parser.add_argument("--input", help="Input NDJSON file (default: stdin)")
    parser.add_argument("--output", help="Output NDJSON file (default: stdout)")
    args = parser.parse_args()

    rules_config = {}
    if args.config:
        with open(args.config, "r") as f:
            rules_config = json.load(f)

    oracle = CostIntelOracle(rules_config)

    in_f = open(args.input, "r") if args.input else sys.stdin
    out_f = open(args.output, "w") if args.output else sys.stdout

    line_num = 1
    for line in in_f:
        rec, fault = oracle.process_record(line, line_num)
        if rec:
            out_f.write(json.dumps(rec) + "\n")
        line_num += 1

    if args.input: in_f.close()
    if args.output: out_f.close()

if __name__ == "__main__":
    main()
