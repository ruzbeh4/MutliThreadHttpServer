#!/usr/bin/env python3
"""Convert ab TSV results to JSON and generate basic plots."""
import csv
import json
import os
import sys
from typing import List, Dict

import matplotlib.pyplot as plt


def load_rows(tsv_path: str) -> List[Dict]:
    rows = []
    with open(tsv_path, newline="") as f:
        reader = csv.DictReader(f, delimiter="\t")
        for r in reader:
            def safe_float(val: str) -> float:
                try:
                    return float(val)
                except Exception:
                    return 0.0

            def safe_int(val: str) -> int:
                try:
                    return int(float(val))
                except Exception:
                    return 0

            rows.append(
                {
                    "threads": int(r["threads"]),
                    "n": int(r["n"]),
                    "c": int(r["c"]),
                    "cache": r.get("cache", "on"),
                    "rps": safe_float(r["rps"]),
                    "time_per_req_ms": safe_float(r["time_per_req_ms"]),
                    "transfer_rate_kBps": safe_float(r["transfer_rate_kBps"]),
                    "failed_requests": safe_int(r["failed_requests"]),
                }
            )
    return rows


def write_json(rows: List[Dict], out_path: str) -> None:
    with open(out_path, "w") as f:
        json.dump(rows, f, indent=2)


def plot_dual_series(xs_on, ys_on, xs_off, ys_off, xlabel, ylabel, title, out_path):
    plt.figure()
    plt.plot(xs_on, ys_on, marker="o", color="navy", label="cache on")
    plt.plot(xs_off, ys_off, marker="o", color="orange", label="cache off")
    plt.xlabel(xlabel)
    plt.ylabel(ylabel)
    plt.title(title)
    plt.legend()
    plt.grid(True, linestyle="--", alpha=0.5)
    plt.tight_layout()
    plt.savefig(out_path)
    plt.close()


def main():
    if len(sys.argv) < 4:
        print("Usage: plot_results.py <results.tsv> <results.json> <out_dir>")
        sys.exit(1)

    tsv_path, json_path, out_dir = sys.argv[1:4]
    os.makedirs(out_dir, exist_ok=True)

    rows = load_rows(tsv_path)
    rows_sorted = sorted(rows, key=lambda r: r["threads"])

    write_json(rows_sorted, json_path)

    rows_on = [r for r in rows_sorted if r.get("cache", "on") != "off"]
    rows_off = [r for r in rows_sorted if r.get("cache", "on") == "off"]

    threads_on = [r["threads"] for r in rows_on]
    rps_on = [r["rps"] for r in rows_on]
    latency_on = [r["time_per_req_ms"] for r in rows_on]
    xfer_on = [r["transfer_rate_kBps"] for r in rows_on]

    threads_off = [r["threads"] for r in rows_off]
    rps_off = [r["rps"] for r in rows_off]
    latency_off = [r["time_per_req_ms"] for r in rows_off]
    xfer_off = [r["transfer_rate_kBps"] for r in rows_off]

    plot_dual_series(threads_on, rps_on, threads_off, rps_off, "Threads", "Requests/sec", "Throughput vs Threads", os.path.join(out_dir, "rps_vs_threads.png"))
    plot_dual_series(threads_on, latency_on, threads_off, latency_off, "Threads", "Time per request (ms)", "Latency vs Threads", os.path.join(out_dir, "latency_vs_threads.png"))
    plot_dual_series(threads_on, xfer_on, threads_off, xfer_off, "Threads", "kB/s", "Transfer rate vs Threads", os.path.join(out_dir, "xfer_vs_threads.png"))

    print(f"Wrote JSON to {json_path}")
    print(f"Plots saved to {out_dir}")


if __name__ == "__main__":
    main()
