#!/usr/bin/env python3
"""PlumbrC gRPC — Lines/sec Benchmark (streaming vs batch vs unary)"""

import sys
import time
import random
import statistics

sys.path.insert(0, "python")  # noqa: E402

import grpc
from plumbrc.proto import plumbr_pb2, plumbr_pb2_grpc

GRPC_HOST = "localhost:50051"

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


def bench_streaming(channel, lines):
    stub = plumbr_pb2_grpc.PlumbrServiceStub(channel)

    def gen():
        for line in lines:
            yield plumbr_pb2.RedactRequest(text=line)

    start = time.perf_counter()
    results = list(stub.RedactStream(gen()))
    elapsed = time.perf_counter() - start

    return len(results), elapsed


def bench_batch(channel, lines, batch_size=200):
    stub = plumbr_pb2_grpc.PlumbrServiceStub(channel)
    total_done = 0
    start = time.perf_counter()

    for i in range(0, len(lines), batch_size):
        batch = lines[i : i + batch_size]
        req = plumbr_pb2.RedactBatchRequest(texts=batch)
        resp = stub.RedactBatch(req)
        total_done += len(resp.results)

    elapsed = time.perf_counter() - start
    return total_done, elapsed


def bench_unary(channel, lines):
    stub = plumbr_pb2_grpc.PlumbrServiceStub(channel)
    total_done = 0
    start = time.perf_counter()

    for line in lines:
        req = plumbr_pb2.RedactRequest(text=line)
        stub.Redact(req)
        total_done += 1

    elapsed = time.perf_counter() - start
    return total_done, elapsed


def main():
    total_lines = int(sys.argv[1]) if len(sys.argv) > 1 else 5000
    host = sys.argv[2] if len(sys.argv) > 2 else GRPC_HOST

    workload = [LOG_LINES[i % len(LOG_LINES)] for i in range(total_lines)]
    random.shuffle(workload)

    print("=" * 64)
    print("  PlumbrC gRPC — Lines/sec Benchmark")
    print("=" * 64)
    print(f"  Host:         {host}")
    print(f"  Total lines:  {total_lines:,}")
    print("=" * 64)

    channel = grpc.insecure_channel(
        host,
        options=[
            ("grpc.max_send_message_length", 4 * 1024 * 1024),
            ("grpc.max_receive_message_length", 4 * 1024 * 1024),
        ],
    )

    # Health check first
    try:
        stub = plumbr_pb2_grpc.PlumbrServiceStub(channel)
        health = stub.Health(plumbr_pb2.HealthRequest(), timeout=5)
        print(f"\n  Server: {health.status} | v{health.version} | "
              f"{health.patterns_loaded} patterns | up {health.uptime_seconds:.0f}s\n")
    except grpc.RpcError as e:
        print(f"\n  ✗ Cannot connect to {host}: {e.code()}\n")
        return

    results = {}

    # 1. Streaming
    print("  [1/3] Bidirectional streaming...", end="", flush=True)
    done, elapsed = bench_streaming(channel, workload)
    lps = done / elapsed if elapsed > 0 else 0
    results["streaming"] = lps
    print(f" {lps:,.0f} lines/sec ({elapsed:.2f}s)")

    # 2. Batch
    print("  [2/3] Batch (batch_size=200)...", end="", flush=True)
    done, elapsed = bench_batch(channel, workload, batch_size=200)
    lps = done / elapsed if elapsed > 0 else 0
    results["batch"] = lps
    print(f" {lps:,.0f} lines/sec ({elapsed:.2f}s)")

    # 3. Unary (limited — only 500 lines)
    unary_count = min(500, total_lines)
    print(f"  [3/3] Unary ({unary_count} lines)...", end="", flush=True)
    done, elapsed = bench_unary(channel, workload[:unary_count])
    lps = done / elapsed if elapsed > 0 else 0
    results["unary"] = lps
    print(f" {lps:,.0f} lines/sec ({elapsed:.2f}s)")

    channel.close()

    # Summary
    print(f"\n{'─' * 64}")
    print(f"  RESULTS — {total_lines:,} lines")
    print(f"{'─' * 64}")
    print(f"  ★ Streaming: {results['streaming']:>10,.0f} lines/sec")
    print(f"    Batch:     {results['batch']:>10,.0f} lines/sec")
    print(f"    Unary:     {results['unary']:>10,.0f} lines/sec")
    print(f"{'=' * 64}")


if __name__ == "__main__":
    main()
