#!/usr/bin/env python3
"""PlumbrC REST API — Lines/sec Benchmark (uses batch endpoint)"""

import requests
import time
import statistics
import sys
import random
import concurrent.futures

API_KEY = "plumbr_live_v5r0AJMUhK35i1inN6xE6LPnM8eD7p1J"
API_URL = "https://plumbr.ca/api/redact/batch"
HEADERS = {
    "Authorization": f"Bearer {API_KEY}",
    "Content-Type": "application/json",
}

# Real-world log lines with embedded PII
LOG_LINES = [
    '192.168.1.42 - john.doe@acme.com [13/Feb/2026:14:23:01 +0000] "GET /api/users HTTP/1.1" 200 1523',
    '10.0.0.15 - admin@internal.corp [13/Feb/2026:14:23:02 +0000] "POST /login HTTP/1.1" 302 0',
    '172.16.0.88 - - [13/Feb/2026:14:23:03 +0000] "GET /health HTTP/1.1" 200 2',
    'ERROR 2026-02-13T14:23:04Z [auth] Failed login for user sarah.connor@skynet.io from 203.0.113.50',
    'WARN 2026-02-13T14:23:05Z [payment] Card validation failed for 4532015112830366 - CVV mismatch',
    'INFO 2026-02-13T14:23:06Z [user-svc] Created account for mike.jones@gmail.com with SSN 345-67-8901',
    "SELECT * FROM users WHERE email = 'alice@wonderland.org' AND api_key = 'AKIAIOSFODNN7EXAMPLE';",
    "INSERT INTO payments (card_number, amount) VALUES ('5425233430109903', 299.99);",
    'aws_access_key_id=AKIAI44QH8DHBEXAMPLE aws_secret_access_key=wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY',
    'Connection from 10.255.0.1 to db-primary.internal:5432 using token ghp_ABCDEFGHijklmnopqrstuvwxyz1234567890',
    'pod/api-server-7d8f9b6c4-xk2p9: Error connecting to redis://admin:p@ssw0rd123@redis-master.default.svc:6379',
    'Step 3/12: Setting GITHUB_TOKEN=ghp_realtoken1234567890abcdefghijklmnop',
    'Feb 13 14:23:10 web01 sshd[12345]: Accepted publickey for deploy from 198.51.100.23 port 22',
    '{"level":"error","ts":"2026-02-13T14:23:12Z","msg":"auth failure","user":"bob@evil.com","ip":"198.51.100.77"}',
    '{"event":"purchase","customer_email":"jane@shop.io","card_last4":"4242","amount":59.99,"ip":"10.0.1.200"}',
    'INFO 2026-02-13T14:23:13Z [health] All systems operational - latency 12ms',
    'DEBUG 2026-02-13T14:23:14Z [cache] Hit ratio: 94.2%, evictions: 37, size: 1.2GB',
    'WARN 2026-02-13T14:23:15Z [queue] Message backlog: 1,247 messages, oldest: 3.2s',
    'INFO Starting application server on port 8080 with 4 workers',
    '2024-01-01 12:00:00 INFO User session created for user_id=42',
    '2024-01-01 12:00:01 DEBUG API response for support@company.com completed in 45ms',
    '2024-01-01 12:00:02 ERROR Config leak: database_url=postgres://admin:secret@db.internal:5432/prod',
    '2024-01-01 12:00:03 INFO Token refresh: Bearer eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiIxMjM0NTY3ODkwIn0.test',
    '2024-01-01 12:00:04 DEBUG Cache miss for key user_profile_123, fetching from origin',
]

session = requests.Session()
session.headers.update(HEADERS)


def send_batch(lines):
    """Send a batch of lines, return (latency_ms, lines_count, success)"""
    start = time.perf_counter()
    try:
        resp = session.post(API_URL, json={"texts": lines}, timeout=30)
        elapsed = (time.perf_counter() - start) * 1000
        if resp.status_code == 200:
            return elapsed, len(lines), True
        return elapsed, 0, False
    except Exception:
        elapsed = (time.perf_counter() - start) * 1000
        return elapsed, 0, False


def run_benchmark(total_lines=1000, batch_size=100, concurrency=1):
    # Build workload
    workload = [LOG_LINES[i % len(LOG_LINES)] for i in range(total_lines)]
    random.shuffle(workload)

    # Split into batches
    batches = [workload[i:i + batch_size] for i in range(0, len(workload), batch_size)]

    print("=" * 64)
    print("  PlumbrC REST API — Lines/sec Benchmark")
    print("=" * 64)
    print(f"  Endpoint:     {API_URL}")
    print(f"  Total lines:  {total_lines:,}")
    print(f"  Batch size:   {batch_size}")
    print(f"  Batches:      {len(batches)}")
    print(f"  Concurrency:  {concurrency}")
    print("=" * 64)

    latencies = []
    total_success_lines = 0
    total_failed = 0

    start_time = time.perf_counter()

    if concurrency == 1:
        for i, batch in enumerate(batches):
            ms, lines, ok = send_batch(batch)
            latencies.append(ms)
            if ok:
                total_success_lines += lines
            else:
                total_failed += 1
            done_lines = total_success_lines + total_failed * batch_size
            print(f"  Batch {i+1}/{len(batches)}: {ms:.0f}ms — "
                  f"{done_lines:,}/{total_lines:,} lines", end="\r")
    else:
        with concurrent.futures.ThreadPoolExecutor(max_workers=concurrency) as pool:
            futures = {pool.submit(send_batch, b): b for b in batches}
            done = 0
            for future in concurrent.futures.as_completed(futures):
                ms, lines, ok = future.result()
                latencies.append(ms)
                if ok:
                    total_success_lines += lines
                else:
                    total_failed += 1
                done += 1
                print(f"  Batch {done}/{len(batches)} done", end="\r")

    wall_time = time.perf_counter() - start_time
    print(" " * 60)

    # Results
    lines_per_sec = total_success_lines / wall_time if wall_time > 0 else 0
    latencies.sort()
    p50 = latencies[len(latencies) // 2]
    p95 = latencies[int(len(latencies) * 0.95)]

    print(f"\n{'─' * 64}")
    print(f"  RESULTS")
    print(f"{'─' * 64}")
    print(f"  Wall time:           {wall_time:.2f}s")
    print(f"  Lines processed:     {total_success_lines:,}/{total_lines:,}")
    print(f"  Failed batches:      {total_failed}")
    print()
    print(f"  ★ THROUGHPUT:        {lines_per_sec:,.0f} lines/sec")
    print()
    print(f"  Batch latency (ms):")
    print(f"    Min:   {latencies[0]:.0f}")
    print(f"    P50:   {p50:.0f}")
    print(f"    P95:   {p95:.0f}")
    print(f"    Max:   {latencies[-1]:.0f}")
    print(f"    Avg:   {statistics.mean(latencies):.0f}")
    print(f"{'=' * 64}")

    return lines_per_sec


if __name__ == "__main__":
    total = int(sys.argv[1]) if len(sys.argv) > 1 else 1000
    batch = int(sys.argv[2]) if len(sys.argv) > 2 else 100
    conc = int(sys.argv[3]) if len(sys.argv) > 3 else 1

    run_benchmark(total_lines=total, batch_size=batch, concurrency=conc)
