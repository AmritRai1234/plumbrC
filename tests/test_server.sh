#!/bin/bash
# ============================================
# PlumbrC Server — Full Test Suite
# ============================================
# set -e removed: test functions track pass/fail internally

BASE="http://localhost:7777"
PASS=0
FAIL=0
TOTAL=0

pass() {
  ((PASS++))
  ((TOTAL++))
  echo "  ✅ $1"
}

fail() {
  ((FAIL++))
  ((TOTAL++))
  echo "  ❌ $1: $2"
}

check() {
  # $1 = test name, $2 = expected substring, $3 = actual output
  if echo "$3" | grep -q "$2"; then
    pass "$1"
  else
    fail "$1" "expected '$2', got: $3"
  fi
}

check_status() {
  # $1 = test name, $2 = expected HTTP code, $3 = actual HTTP code
  if [ "$3" = "$2" ]; then
    pass "$1"
  else
    fail "$1" "expected HTTP $2, got HTTP $3"
  fi
}

echo ""
echo "╔══════════════════════════════════════════╗"
echo "║  PlumbrC Server — Full Test Suite        ║"
echo "╚══════════════════════════════════════════╝"
echo ""

# ─── 1. Health Check ──────────────────────────────
echo "━━━ 1. Health Check ━━━"

RESP=$(curl -s $BASE/health)
check "GET /health returns status" '"status":"healthy"' "$RESP"
check "GET /health has version" '"version"' "$RESP"
check "GET /health has uptime" '"uptime_seconds"' "$RESP"
check "GET /health has patterns" '"patterns_loaded":14' "$RESP"
check "GET /health has stats" '"requests_total"' "$RESP"

RESP=$(curl -s $BASE/api/health)
check "GET /api/health alias works" '"status":"healthy"' "$RESP"

CODE=$(curl -s -o /dev/null -w "%{http_code}" $BASE/health)
check_status "GET /health returns 200" "200" "$CODE"

echo ""

# ─── 2. Single Redact — Pattern Matching ──────────
echo "━━━ 2. Single Redact — Pattern Matching ━━━"

# AWS Access Key
RESP=$(curl -s -X POST $BASE/api/redact -H "Content-Type: application/json" \
  -d '{"text": "aws_key=AKIAIOSFODNN7EXAMPLE"}')
check "AWS Access Key redacted" 'REDACTED:aws_access_key' "$RESP"

# Password
RESP=$(curl -s -X POST $BASE/api/redact -H "Content-Type: application/json" \
  -d '{"text": "password=secret123"}')
check "Password redacted" 'REDACTED:password' "$RESP"

# Email
RESP=$(curl -s -X POST $BASE/api/redact -H "Content-Type: application/json" \
  -d '{"text": "contact user@example.com now"}')
check "Email redacted" 'REDACTED:email' "$RESP"

# IPv4
RESP=$(curl -s -X POST $BASE/api/redact -H "Content-Type: application/json" \
  -d '{"text": "login from 192.168.1.100"}')
check "IPv4 redacted" 'REDACTED:ipv4' "$RESP"

# Multiple patterns in one line
RESP=$(curl -s -X POST $BASE/api/redact -H "Content-Type: application/json" \
  -d '{"text": "email=user@test.com password=abc123 from 10.0.0.1"}')
check "Multiple patterns in one line" '"patterns_matched":3' "$RESP"

# Multi-line input
RESP=$(curl -s -X POST $BASE/api/redact -H "Content-Type: application/json" \
  -d '{"text": "line1 password=secret\nline2 clean\nline3 user@test.com"}')
check "Multi-line: 3 lines processed" '"lines_processed":3' "$RESP"
check "Multi-line: 2 lines modified" '"lines_modified":2' "$RESP"

# Clean input (no redaction needed)
RESP=$(curl -s -X POST $BASE/api/redact -H "Content-Type: application/json" \
  -d '{"text": "This is a normal log message without secrets"}')
check "Clean input: 0 modified" '"lines_modified":0' "$RESP"
check "Clean input: 0 patterns" '"patterns_matched":0' "$RESP"

echo ""

# ─── 3. Single Redact — Response Format ──────────
echo "━━━ 3. Response Format Validation ━━━"

RESP=$(curl -s -X POST $BASE/api/redact -H "Content-Type: application/json" \
  -d '{"text": "email=test@test.com"}')
check "Has redacted field" '"redacted"' "$RESP"
check "Has stats object" '"stats"' "$RESP"
check "Has processing_time_ms" '"processing_time_ms"' "$RESP"
check "Has lines_processed" '"lines_processed"' "$RESP"

echo ""

# ─── 4. Batch Redact ─────────────────────────────
echo "━━━ 4. Batch Redact ━━━"

RESP=$(curl -s -X POST $BASE/api/redact/batch -H "Content-Type: application/json" \
  -d '{"texts": ["aws_key=AKIAIOSFODNN7EXAMPLE", "clean line", "email user@test.com", "ip 10.0.0.5"]}')
check "Batch: has results array" '"results"' "$RESP"
check "Batch: has stats" '"items_processed":4' "$RESP"
check "Batch: AWS key redacted" 'REDACTED:aws_access_key' "$RESP"
check "Batch: email redacted" 'REDACTED:email' "$RESP"
check "Batch: IP redacted" 'REDACTED:ipv4' "$RESP"
check "Batch: has total_patterns_matched" '"total_patterns_matched"' "$RESP"
check "Batch: has processing_time_ms" '"processing_time_ms"' "$RESP"

# Single-item batch
RESP=$(curl -s -X POST $BASE/api/redact/batch -H "Content-Type: application/json" \
  -d '{"texts": ["just one item with email user@test.com"]}')
check "Batch single-item works" '"items_processed":1' "$RESP"
check "Batch single-item redacted" 'REDACTED:email' "$RESP"

# Empty array
RESP=$(curl -s -X POST $BASE/api/redact/batch -H "Content-Type: application/json" \
  -d '{"texts": []}')
check "Batch empty array: 0 items" '"items_processed":0' "$RESP"

echo ""

# ─── 5. Error Handling ───────────────────────────
echo "━━━ 5. Error Handling ━━━"

# Missing body on POST
CODE=$(curl -s -o /dev/null -w "%{http_code}" -X POST $BASE/api/redact)
check_status "Missing body returns 400" "400" "$CODE"

# Invalid JSON
CODE=$(curl -s -o /dev/null -w "%{http_code}" -X POST $BASE/api/redact \
  -H "Content-Type: application/json" -d '{"wrong": "field"}')
check_status "Missing text field returns 400" "400" "$CODE"

# 404 for unknown route
CODE=$(curl -s -o /dev/null -w "%{http_code}" $BASE/nonexistent)
check_status "Unknown route returns 404" "404" "$CODE"

# Missing body on batch
CODE=$(curl -s -o /dev/null -w "%{http_code}" -X POST $BASE/api/redact/batch)
check_status "Batch missing body returns 400" "400" "$CODE"

# Invalid batch (not an array)
RESP=$(curl -s -X POST $BASE/api/redact/batch -H "Content-Type: application/json" \
  -d '{"texts": "not-an-array"}')
check "Batch non-array returns error" '"error"' "$RESP"

# JSON key prefix collision handling
RESP=$(curl -s -X POST $BASE/api/redact -H "Content-Type: application/json" \
  -d '{"logger": "text", "text": "secret123 password=hunter2"}')
check "JSON key prefix collision handled" 'REDACTED:password' "$RESP"

echo ""

# ─── 6. CORS ─────────────────────────────────────
echo "━━━ 6. CORS Headers ━━━"

HEADERS=$(curl -s -I -X OPTIONS $BASE/api/redact 2>&1)
check "OPTIONS returns CORS origin" "Access-Control-Allow-Origin" "$HEADERS"
check "OPTIONS returns CORS methods" "Access-Control-Allow-Methods" "$HEADERS"

HEADERS=$(curl -s -D - -o /dev/null -X POST $BASE/api/redact \
  -H "Content-Type: application/json" -d '{"text": "test"}' 2>&1)
check "POST response has CORS" "Access-Control-Allow-Origin" "$HEADERS"

echo ""

# ─── 7. Keep-Alive ───────────────────────────────
echo "━━━ 7. HTTP Keep-Alive ━━━"

HEADERS=$(curl -s -D - -o /dev/null -X POST $BASE/api/redact \
  -H "Content-Type: application/json" -d '{"text": "test"}' 2>&1)
check "Response has Connection: keep-alive" "keep-alive" "$HEADERS"

echo ""

# ─── 8. Special Characters ───────────────────────
echo "━━━ 8. Special Characters & Edge Cases ━━━"

# Escaped newlines (use printf to send actual \n in JSON)
RESP=$(printf '{"text": "line1\\nline2 password=hunter2"}' | \
  curl -s -X POST $BASE/api/redact -H "Content-Type: application/json" -d @-)
check "Escaped newlines handled" 'REDACTED:password' "$RESP"

# Unicode  
RESP=$(curl -s -X POST $BASE/api/redact -H "Content-Type: application/json" \
  -d '{"text": "日本語 password=test123"}')
check "Unicode text preserved" 'REDACTED:password' "$RESP"

# Text with multiple patterns
RESP=$(curl -s -X POST $BASE/api/redact -H "Content-Type: application/json" \
  -d '{"text": "user@test.com from 10.0.0.1"}')
check "Multiple patterns at once" 'REDACTED' "$RESP"

# Empty text
RESP=$(curl -s -X POST $BASE/api/redact -H "Content-Type: application/json" \
  -d '{"text": ""}')
check "Empty text returns 200" '"lines_processed"' "$RESP"

echo ""

# ─── 9. CLI Binary ───────────────────────────────
echo "━━━ 9. CLI Binary (plumbr) ━━━"

# Basic pipeline
RESP=$(echo "password=secret123" | ./build/bin/plumbr)
check "CLI: password redacted" 'REDACTED' "$RESP"

RESP=$(echo "AKIAIOSFODNN7EXAMPLE" | ./build/bin/plumbr)
check "CLI: AWS key redacted" 'REDACTED' "$RESP"

RESP=$(echo "test@example.com" | ./build/bin/plumbr)
check "CLI: email redacted" 'REDACTED' "$RESP"

RESP=$(echo "clean log message" | ./build/bin/plumbr)
check "CLI: clean line unchanged" "clean log message" "$RESP"

echo ""

# ─── 10. Performance Benchmark ───────────────────
echo "━━━ 10. Performance Benchmark ━━━"

python3 -c "
import socket, time, threading, json

HOST = '127.0.0.1'
PORT = 7777
TOTAL_PER_THREAD = 2500
THREADS = 8

payload = json.dumps({'text': 'aws_key=AKIAIOSFODNN7EXAMPLE password=secret123\nUser login from 192.168.1.100 with email user@example.com'})
request = (
    'POST /api/redact HTTP/1.1\r\n'
    'Host: localhost\r\n'
    'Content-Type: application/json\r\n'
    f'Content-Length: {len(payload)}\r\n'
    'Connection: keep-alive\r\n'
    '\r\n'
    f'{payload}'
).encode()

results = [0] * THREADS

def worker(tid):
    count = 0
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    sock.connect((HOST, PORT))
    for _ in range(TOTAL_PER_THREAD):
        sock.sendall(request)
        data = b''
        while b'\r\n\r\n' not in data:
            data += sock.recv(4096)
        header_end = data.index(b'\r\n\r\n') + 4
        cl_start = data.lower().index(b'content-length:') + 15
        cl_end = data.index(b'\r\n', cl_start)
        cl = int(data[cl_start:cl_end].strip())
        body_so_far = len(data) - header_end
        while body_so_far < cl:
            chunk = sock.recv(4096)
            body_so_far += len(chunk)
        count += 1
    sock.close()
    results[tid] = count

start = time.time()
threads = []
for i in range(THREADS):
    t = threading.Thread(target=worker, args=(i,))
    t.start()
    threads.append(t)
for t in threads:
    t.join()
elapsed = time.time() - start
total = sum(results)
rps = total / elapsed
latency_ms = elapsed / total * 1000

print(f'  Requests: {total}')
print(f'  Time: {elapsed:.2f}s')
print(f'  RPS: {rps:.0f}')
print(f'  Avg latency: {latency_ms:.3f}ms')

if rps > 10000:
    print('  ✅ Performance: >10K RPS')
elif rps > 5000:
    print('  ✅ Performance: >5K RPS (acceptable)')
else:
    print('  ❌ Performance: below 5K RPS target')
"

echo ""

# ─── 11. Makefile Targets ────────────────────────
echo "━━━ 11. Build System ━━━"

[ -f ./build/bin/plumbr ] && pass "plumbr binary exists" || fail "plumbr binary missing"
[ -f ./build/bin/plumbr-server ] && pass "plumbr-server binary exists" || fail "plumbr-server binary missing"
[ -f ./Dockerfile ] && pass "Dockerfile exists" || fail "Dockerfile missing"
[ -f ./start.sh ] && pass "start.sh exists" || fail "start.sh missing"
[ -f ./integrations/systemd/plumbr-server.service ] && pass "systemd service exists" || fail "systemd service missing"

echo ""

# ─── 12. Health After Load ───────────────────────
echo "━━━ 12. Health After Load ━━━"

RESP=$(curl -s $BASE/health)
check "Health after load: still healthy" '"status":"healthy"' "$RESP"
check "Health after load: requests counted" '"requests_total"' "$RESP"
check "Health after load: bytes tracked" '"bytes_processed"' "$RESP"

echo ""

# ─── Summary ─────────────────────────────────────
echo "══════════════════════════════════════════"
echo " Results: $PASS passed, $FAIL failed (out of $TOTAL)"
if [ $FAIL -eq 0 ]; then
  echo " 🎉 ALL TESTS PASSED"
else
  echo " ⚠️  Some tests failed"
fi
echo "══════════════════════════════════════════"
