import os
import csv
import glob

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
OUTPUT_DIR = os.path.join(BASE_DIR, "output_files")

SCHEDULERS = ["EP", "RR", "EP_RR"]


def parse_log(path):
    entries = []
    with open(path, "r") as f:
        for line in f:
            line = line.rstrip("\n")
            if not line.strip().startswith("|"):
                continue
            if "Time of Transition" in line or "---" in line:
                continue

            parts = [p.strip() for p in line.split("|") if p.strip()]
            if len(parts) != 4:
                continue

            try:
                t = int(parts[0])
                pid = int(parts[1])
                old = parts[2]
                new = parts[3]
            except ValueError:
                continue

            entries.append((t, pid, old, new))

    entries.sort(key=lambda x: x[0])
    return entries


def compute_metrics(entries):
    if not entries:
        return {
            "throughput": 0.0,
            "avg_wait": 0.0,
            "avg_turnaround": 0.0,
            "avg_response": 0.0,
            "num_procs": 0,
        }

    pids = sorted(set(pid for _, pid, _, _ in entries))
    proc = {}
    for pid in pids:
        proc[pid] = {
            "arrival": None,
            "finish": None,
            "wait": 0,
            "ready_since": None,
        }

    io_wait_start = {}
    io_response_times = []

    for t, pid, old, new in entries:
        info = proc[pid]

        if old == "NEW" and new == "READY" and info["arrival"] is None:
            info["arrival"] = t

        if new == "READY":
            info["ready_since"] = t

        if old == "READY" and new == "RUNNING" and info["ready_since"] is not None:
            info["wait"] += t - info["ready_since"]
            info["ready_since"] = None

        if new == "TERMINATED":
            info["finish"] = t

        if old == "RUNNING" and new == "WAITING":
            io_wait_start[pid] = t
        if old == "WAITING" and new == "READY":
            if pid in io_wait_start:
                io_response_times.append(t - io_wait_start[pid])
                del io_wait_start[pid]

    finishes = [v["finish"] for v in proc.values() if v["finish"] is not None]
    if finishes:
        total_time = max(finishes)
        throughput = len(finishes) / total_time if total_time > 0 else 0.0
    else:
        throughput = 0.0

    waits = []
    turnarounds = []

    for pid, info in proc.items():
        if info["arrival"] is not None and info["finish"] is not None:
            waits.append(info["wait"])
            turnarounds.append(info["finish"] - info["arrival"])

    avg_wait = sum(waits) / len(waits) if waits else 0.0
    avg_turnaround = sum(turnarounds) / len(turnarounds) if turnarounds else 0.0
    avg_response = sum(io_response_times) / len(io_response_times) if io_response_times else 0.0

    return {
        "throughput": throughput,
        "avg_wait": avg_wait,
        "avg_turnaround": avg_turnaround,
        "avg_response": avg_response,
        "num_procs": len(pids),
    }


def main():
    metrics_path = os.path.join(BASE_DIR, "metrics_summary.csv")

    with open(metrics_path, "w", newline="") as csvfile:
        fields = [
            "scheduler",
            "trace",
            "num_processes",
            "throughput",
            "avg_wait",
            "avg_turnaround",
            "avg_response",
        ]
        writer = csv.DictWriter(csvfile, fieldnames=fields)
        writer.writeheader()

        for sched in SCHEDULERS:
            pattern = os.path.join(OUTPUT_DIR, f"{sched}_*.txt")
            for log_path in glob.glob(pattern):
                log_name = os.path.basename(log_path)

                entries = parse_log(log_path)
                m = compute_metrics(entries)

                writer.writerow({
                    "scheduler": sched,
                    "trace": log_name,
                    "num_processes": m["num_procs"],
                    "throughput": f"{m['throughput']:.4f}",
                    "avg_wait": f"{m['avg_wait']:.2f}",
                    "avg_turnaround": f"{m['avg_turnaround']:.2f}",
                    "avg_response": f"{m['avg_response']:.2f}",
                })

    print(f"Metrics written to {metrics_path}")


if __name__ == "__main__":
    main()

