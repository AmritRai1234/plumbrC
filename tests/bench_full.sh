#!/bin/bash
# PlumbrC Full Benchmark — 702 patterns, real-world workload
# Tests throughput across multiple data sizes and thread counts

set -euo pipefail

BINARY="./build/bin/plumbr"
PATTERNS="./patterns/all.txt"
TEST_DIR="./test_data"
RESULTS_FILE="./test_data/benchmark_results.txt"

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

mkdir -p "$TEST_DIR"

echo -e "${BOLD}╔══════════════════════════════════════════════════╗${NC}"
echo -e "${BOLD}║  PlumbrC Full Benchmark — 702 Patterns           ║${NC}"
echo -e "${BOLD}╚══════════════════════════════════════════════════╝${NC}"
echo ""

# ─── Generate realistic test data ─────────────────────────────
generate_test_data() {
    local lines=$1
    local output=$2
    echo -e "${CYAN}Generating $lines lines of realistic log data...${NC}"

    python3 -c "
import random, sys

# Realistic log lines with secrets embedded at ~15% rate
normal_lines = [
    '2024-01-15 08:30:12.456 INFO  [main] Application started successfully on port 8080',
    '2024-01-15 08:30:13.789 DEBUG [pool-1] Processing request GET /api/v2/users?page=1&limit=25',
    '2024-01-15 08:30:14.012 INFO  [http-nio-8080] User session created: sid=abc123def456',
    '2024-01-15 08:30:14.234 DEBUG [cache] Cache HIT for key user_profile_9382 (ttl=3600s)',
    '2024-01-15 08:30:14.567 INFO  [scheduler] Cron job daily-cleanup completed in 2.3s',
    '2024-01-15 08:30:15.890 WARN  [pool-3] Connection pool exhausted, waiting 500ms',
    '2024-01-15 08:30:16.123 INFO  [gateway] Upstream response 200 OK in 45ms',
    '2024-01-15 08:30:16.456 DEBUG [hibernate] SELECT * FROM orders WHERE status=PENDING LIMIT 100',
    '2024-01-15 08:30:17.789 INFO  [metrics] cpu=23.4% mem=1.2GB gc_pause=3ms heap=890MB',
    '2024-01-15 08:30:18.012 INFO  [audit] User admin@internal performed action DEPLOY',
    '2024-01-15 08:30:18.345 DEBUG [kafka] Published event OrderCreated to topic orders.events',
    '2024-01-15 08:30:19.678 INFO  [health] All 12 health checks passed in 156ms',
    '2024-01-15 08:30:20.901 DEBUG [redis] SET session:u9382 EX 3600 (1.2ms)',
    '2024-01-15 08:30:21.234 INFO  [lb] Backend server-03 healthy, latency=12ms',
    '2024-01-15 08:30:22.567 WARN  [circuit] Circuit breaker payment-service HALF_OPEN',
]

secret_lines = [
    # AWS keys
    '2024-01-15 09:12:33.456 ERROR [aws] Failed auth with key AKIAIOSFODNN7EXAMPLE for s3://prod-bucket',
    '2024-01-15 09:12:34.789 DEBUG [config] aws_secret_access_key = wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY',
    # GitHub tokens
    '2024-01-15 09:15:22.123 INFO  [deploy] Cloning with token ghp_ABCDEFGHIJKLMNOPQRSTUVWXYZabcdef1234',
    # Passwords
    '2024-01-15 09:18:45.456 ERROR [auth] Login failed password=SuperSecret123! for user john',
    '2024-01-15 09:18:46.789 DEBUG [config] database.password = MyDBPass!2024',
    # API keys
    '2024-01-15 09:20:12.012 DEBUG [stripe] Charging with api_key=sk_test_EXAMPLE_KEY_00000000000000',
    # JWT tokens
    '2024-01-15 09:22:33.345 DEBUG [auth] JWT: eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiIxMjM0NTY3ODkwIiwibmFtZSI6IkpvaG4gRG9lIiwiaWF0IjoxNTE2MjM5MDIyfQ.SflKxwRJSMeKKF2QT4fwpMeJf36POk6yJV_adQssw5c',
    # Emails
    '2024-01-15 09:25:44.678 INFO  [notify] Sent welcome email to john.doe@example.com',
    '2024-01-15 09:25:45.901 INFO  [support] Ticket created by admin@company.org ref=TKT-9821',
    # IPs
    '2024-01-15 09:28:56.234 WARN  [firewall] Blocked suspicious request from 192.168.1.100',
    '2024-01-15 09:28:57.567 INFO  [proxy] Request forwarded to upstream 10.0.0.42:8080',
    # Slack tokens
    '2024-01-15 09:30:08.890 DEBUG [slack] Posting to channel with xoxb-0000000000-EXAMPLETOKEN',
    # Private keys
    '2024-01-15 09:32:19.123 ERROR [tls] Certificate error -----BEGIN RSA PRIVATE KEY-----',
    # Cloud
    '2024-01-15 09:34:30.456 DEBUG [gcp] Using API key AIzaSyABC_DEF-GHI_JKL-MNO_PQR_STU_VWXYZ12',
    # Credit cards
    '2024-01-15 09:36:41.789 ERROR [payment] Card validation failed for 4532-1234-5678-9012',
    # Connection strings
    '2024-01-15 09:38:52.012 DEBUG [db] Connecting: postgresql://admin:secretpass@db.internal:5432/production',
    # SSN
    '2024-01-15 09:40:03.345 WARN  [pii] PII detected in payload: SSN 123-45-6789',
    # Azure
    '2024-01-15 09:42:14.678 DEBUG [azure] Using connection DefaultEndpointsProtocol=https;AccountName=myaccount;AccountKey=ABCDEFGHIJKLMNOP+abcdefghijklmnop/1234567890ABCDEFGHIJKLMNOP+abcdefghijklmno==',
    # Redis
    '2024-01-15 09:44:25.901 DEBUG [redis] AUTH password=RedisP@ss2024',
    # MongoDB
    '2024-01-15 09:46:37.234 DEBUG [mongo] mongodb://admin:MongoSecret@mongo.internal:27017/prod?authSource=admin',
]

random.seed(42)  # Reproducible
for i in range($lines):
    if random.random() < 0.15:
        print(random.choice(secret_lines))
    else:
        print(random.choice(normal_lines))
" > "$output"

    local size=$(du -h "$output" | cut -f1)
    local count=$(wc -l < "$output")
    echo -e "  Generated: ${GREEN}$count lines${NC}, ${GREEN}$size${NC}"
}

# ─── Run single benchmark ─────────────────────────────────────
run_bench() {
    local name=$1
    local input=$2
    local threads=$3

    local lines=$(wc -l < "$input")
    local size_bytes=$(stat --format=%s "$input")
    local size_mb=$(echo "scale=2; $size_bytes / 1048576" | bc)

    echo -e "${YELLOW}  [$name] threads=$threads, lines=$lines, size=${size_mb}MB${NC}"

    # Run 3 times and take the best
    local best_lps=0
    local best_mbps=0
    local best_elapsed=999

    for run in 1 2 3; do
        local stats=$($BINARY -p "$PATTERNS" -D -j "$threads" -q -s < "$input" > /dev/null 2>&1 || true)

        # Run with stats captured to stderr
        local output=$($BINARY -p "$PATTERNS" -D -j "$threads" -s < "$input" 2>&1 > /dev/null)

        local lps=$(echo "$output" | grep "Throughput:" | head -1 | grep -oP '[0-9,]+' | tr -d ',')
        local mbps=$(echo "$output" | grep "Throughput:" | tail -1 | grep -oP '[0-9.]+')
        local elapsed=$(echo "$output" | grep "Elapsed" | grep -oP '[0-9.]+')
        local matched=$(echo "$output" | grep "Patterns matched" | grep -oP '[0-9,]+' | tr -d ',')
        local modified=$(echo "$output" | grep "Lines modified" | grep -oP '[0-9,]+' | head -1 | tr -d ',')

        if [ -n "$lps" ] && [ "$lps" -gt "$best_lps" ] 2>/dev/null; then
            best_lps=$lps
            best_mbps=$mbps
            best_elapsed=$elapsed
            best_matched=$matched
            best_modified=$modified
        fi
    done

    printf "    → %s lines/sec | %s MB/sec | %ss | %s matches | %s modified\n" \
        "$best_lps" "$best_mbps" "$best_elapsed" "${best_matched:-0}" "${best_modified:-0}"

    echo "$name|$threads|$lines|$size_mb|$best_lps|$best_mbps|$best_elapsed|${best_matched:-0}|${best_modified:-0}" >> "$RESULTS_FILE"
}

# ─── Main ─────────────────────────────────────────────────────

# Generate test data at different sizes
generate_test_data 100000  "$TEST_DIR/bench_100k.log"
generate_test_data 500000  "$TEST_DIR/bench_500k.log"
generate_test_data 1000000 "$TEST_DIR/bench_1m.log"

echo ""
echo -e "${BOLD}=== Baseline Benchmark Results ===${NC}"
echo "name|threads|lines|size_mb|lines_per_sec|mb_per_sec|elapsed|matches|modified" > "$RESULTS_FILE"

# Single-threaded baseline
echo -e "\n${CYAN}── Single-threaded (j=1) ──${NC}"
run_bench "100K-1T" "$TEST_DIR/bench_100k.log" 1
run_bench "500K-1T" "$TEST_DIR/bench_500k.log" 1
run_bench "1M-1T"   "$TEST_DIR/bench_1m.log"   1

# Auto threads
echo -e "\n${CYAN}── Auto threads (j=0) ──${NC}"
run_bench "100K-auto" "$TEST_DIR/bench_100k.log" 0
run_bench "500K-auto" "$TEST_DIR/bench_500k.log" 0
run_bench "1M-auto"   "$TEST_DIR/bench_1m.log"   0

# Specific thread counts
echo -e "\n${CYAN}── Thread scaling (1M lines) ──${NC}"
for t in 2 4 6 8 12; do
    run_bench "1M-${t}T" "$TEST_DIR/bench_1m.log" $t
done

echo ""
echo -e "${BOLD}=== Results saved to $RESULTS_FILE ===${NC}"
echo ""
cat "$RESULTS_FILE" | column -t -s'|'
echo ""
echo -e "${GREEN}Benchmark complete!${NC}"
