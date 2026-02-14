#!/usr/bin/env python3
"""Real-world workload benchmark for PlumbrC REST API"""

import requests
import time
import statistics
import concurrent.futures
import random
import sys

API_KEY = "plumbr_live_v5r0AJMUhK35i1inN6xE6LPnM8eD7p1J"
API_URL = "https://plumbr.ca/api/redact"
HEADERS = {
    "Authorization": f"Bearer {API_KEY}",
    "Content-Type": "application/json",
}

# ── Real-world log lines with embedded PII ──────────────────────────
WORKLOAD = [
    # Web server access logs
    '192.168.1.42 - john.doe@acme.com [13/Feb/2026:14:23:01 +0000] "GET /api/users HTTP/1.1" 200 1523',
    '10.0.0.15 - admin@internal.corp [13/Feb/2026:14:23:02 +0000] "POST /login HTTP/1.1" 302 0',
    '172.16.0.88 - - [13/Feb/2026:14:23:03 +0000] "GET /health HTTP/1.1" 200 2',
    
    # Application error logs
    'ERROR 2026-02-13T14:23:04Z [auth] Failed login for user sarah.connor@skynet.io from 203.0.113.50 - invalid password',
    'WARN 2026-02-13T14:23:05Z [payment] Card validation failed for 4532015112830366 - CVV mismatch',
    'INFO 2026-02-13T14:23:06Z [user-svc] Created account for mike.jones@gmail.com with SSN 345-67-8901',
    
    # Database query logs
    "SELECT * FROM users WHERE email = 'alice@wonderland.org' AND api_key = 'AKIAIOSFODNN7EXAMPLE';",
    "INSERT INTO payments (card_number, amount) VALUES ('5425233430109903', 299.99);",
    "UPDATE users SET phone = '(555) 234-5678' WHERE id = 42;",
    
    # Cloud infrastructure logs
    'aws_access_key_id=AKIAI44QH8DHBEXAMPLE aws_secret_access_key=wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY',
    'Connection from 10.255.0.1 to db-primary.internal:5432 using token ghp_ABCDEFGHijklmnopqrstuvwxyz1234567890',
    'Deploying to server 52.14.203.99 with SSH key fingerprint SHA256:abcdef1234567890',
    
    # Kubernetes / Docker logs
    'pod/api-server-7d8f9b6c4-xk2p9: Error connecting to redis://admin:p@ssw0rd123@redis-master.default.svc:6379',
    'container nginx-proxy started, binding 0.0.0.0:443, upstream 172.17.0.5:8080',
    
    # CI/CD pipeline logs
    'Step 3/12: Setting GITHUB_TOKEN=ghp_realtoken1234567890abcdefghijklmnop',
    'Pushing image to registry.acme.com with credentials admin@acme.com / sk_live_51234567890abcdefghij',
    
    # Syslog / auth logs
    'Feb 13 14:23:10 web01 sshd[12345]: Accepted publickey for deploy from 198.51.100.23 port 22',
    'Feb 13 14:23:11 web01 sudo: admin : TTY=pts/0 ; PWD=/home/admin ; USER=root ; COMMAND=/bin/systemctl restart nginx',
    
    # JSON structured logs
    '{"level":"error","ts":"2026-02-13T14:23:12Z","msg":"auth failure","user":"bob@evil.com","ip":"198.51.100.77","attempt":5}',
    '{"event":"purchase","customer_email":"jane@shop.io","card_last4":"4242","amount":59.99,"ip":"10.0.1.200"}',
    
    # Clean lines (no PII)
    'INFO 2026-02-13T14:23:13Z [health] All systems operational - latency 12ms',
    'DEBUG 2026-02-13T14:23:14Z [cache] Hit ratio: 94.2%, evictions: 37, size: 1.2GB',
    'WARN 2026-02-13T14:23:15Z [queue] Message backlog: 1,247 messages, oldest: 3.2s',
    'INFO Starting application server on port 8080 with 4 workers',
]

session = requests.Session()
session.headers.update(HEADERS)


def send_request(text):
    """Send a single redact request, return (latency_ms, success, redacted_text)"""
    start = time.perf_counter()
    try:
        resp = session.post(API_URL, json={"text": text}, timeout=15)
        elapsed = (time.perf_counter() - start) * 1000
        if resp.status_code == 200:
            data = resp.json()
            return elapsed, True, data.get("redacted", data.get("result", ""))
        return elapsed, False, f"HTTP {resp.status_code}"
    except Exception as e:
        elapsed = (time.perf_counter() - start) * 1000
        return elapsed, False, str(e)


def run_benchmark(total_requests=100, concurrency=1):
    """Run the benchmark"""
    # Build the workload by cycling through real-world lines
    workload = [WORKLOAD[i % len(WORKLOAD)] for i in range(total_requests)]
    random.shuffle(workload)

    print("=" * 64)
    print("  PlumbrC REST API — Real-World Workload Benchmark")
    print("=" * 64)
    print(f"  Endpoint:     {API_URL}")
    print(f"  Requests:     {total_requests}")
    print(f"  Concurrency:  {concurrency}")
    print(f"  Log lines:    {len(WORKLOAD)} unique templates")
    print("=" * 64)

    latencies = []
    successes = 0
    failures = 0
    redaction_counts = 0

    start_time = time.perf_counter()

    if concurrency == 1:
        # Sequential
        for i, text in enumerate(workload):
            ms, ok, result = send_request(text)
            latencies.append(ms)
            if ok:
                successes += 1
                if "[REDACTED" in result:
                    redaction_counts += 1
            else:
                failures += 1
            # Progress
            if (i + 1) % 10 == 0 or i == 0:
                print(f"  Progress: {i+1}/{total_requests} "
                      f"({(i+1)/total_requests*100:.0f}%) — "
                      f"last: {ms:.0f}ms", end="\r")
    else:
        # Concurrent
        with concurrent.futures.ThreadPoolExecutor(max_workers=concurrency) as pool:
            futures = {pool.submit(send_request, t): t for t in workload}
            done = 0
            for future in concurrent.futures.as_completed(futures):
                ms, ok, result = future.result()
                latencies.append(ms)
                if ok:
                    successes += 1
                    if "[REDACTED" in result:
                        redaction_counts += 1
                else:
                    failures += 1
                done += 1
                if done % 10 == 0 or done == 1:
                    print(f"  Progress: {done}/{total_requests} "
                          f"({done/total_requests*100:.0f}%)", end="\r")

    wall_time = time.perf_counter() - start_time
    print(" " * 60)  # clear progress line

    # ── Results ──
    latencies.sort()
    p50 = latencies[len(latencies) // 2]
    p95 = latencies[int(len(latencies) * 0.95)]
    p99 = latencies[int(len(latencies) * 0.99)]

    rps = successes / wall_time if wall_time > 0 else 0

    print(f"\n{'─' * 64}")
    print(f"  RESULTS")
    print(f"{'─' * 64}")
    print(f"  Total time:       {wall_time:.2f}s")
    print(f"  Successful:       {successes}/{total_requests}")
    print(f"  Failed:           {failures}/{total_requests}")
    print(f"  Lines with PII:   {redaction_counts}/{successes} had redactions")
    print()
    print(f"  Throughput:       {rps:.1f} req/s")
    print()
    print(f"  Latency (ms):")
    print(f"    Min:    {latencies[0]:.0f}")
    print(f"    P50:    {p50:.0f}")
    print(f"    P95:    {p95:.0f}")
    print(f"    P99:    {p99:.0f}")
    print(f"    Max:    {latencies[-1]:.0f}")
    print(f"    Avg:    {statistics.mean(latencies):.0f}")
    print(f"    StdDev: {statistics.stdev(latencies):.0f}" if len(latencies) > 1 else "")
    print(f"{'=' * 64}")


if __name__ == "__main__":
    total = int(sys.argv[1]) if len(sys.argv) > 1 else 50
    conc = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    run_benchmark(total_requests=total, concurrency=conc)
