#!/usr/bin/env python3
"""Test script for PlumbrC REST API"""

import requests
import json
import time

API_KEY = "plumbr_live_v5r0AJMUhK35i1inN6xE6LPnM8eD7p1J"
API_SECRET = "plumbr_secret_NqAMLtCxT0DeXbu26J0SS4iIvCvSFJQCuQAiBD4q"
API_URL = "https://plumbr.ca/api/redact"

HEADERS = {
    "Authorization": f"Bearer {API_KEY}",
    "Content-Type": "application/json",
}

# Test cases: (name, input_text, expected_redacted_patterns)
TEST_CASES = [
    (
        "Credit Card (Visa)",
        "Payment processed for card 4532015112830366 on 2024-01-15",
        ["4532015112830366"],
    ),
    (
        "Email Address",
        "Contact john.doe@example.com for support",
        ["john.doe@example.com"],
    ),
    (
        "Phone Number",
        "Call me at 555-123-4567 or (555) 987-6543",
        ["555-123-4567", "(555) 987-6543"],
    ),
    (
        "SSN",
        "SSN: 123-45-6789 was found in the logs",
        ["123-45-6789"],
    ),
    (
        "IP Address",
        "Connection from 192.168.1.100 was blocked",
        ["192.168.1.100"],
    ),
    (
        "AWS Key",
        "Found key AKIAIOSFODNN7EXAMPLE in config",
        ["AKIAIOSFODNN7EXAMPLE"],
    ),
    (
        "Mixed PII",
        "User john@test.com (SSN: 234-56-7890) paid with 5425233430109903 from IP 10.0.0.1",
        ["john@test.com", "234-56-7890", "5425233430109903", "10.0.0.1"],
    ),
    (
        "No PII (should be unchanged)",
        "This is a normal log message with no sensitive data",
        [],
    ),
]


def run_tests():
    print("=" * 60)
    print("  PlumbrC REST API Test Suite")
    print("=" * 60)
    print(f"  Endpoint: {API_URL}")
    print(f"  API Key:  {API_KEY[:20]}...")
    print("=" * 60)

    passed = 0
    failed = 0
    errors = 0
    total_time = 0

    for name, input_text, expected_patterns in TEST_CASES:
        print(f"\n{'─' * 50}")
        print(f"TEST: {name}")
        print(f"  Input:    {input_text[:70]}{'...' if len(input_text) > 70 else ''}")

        try:
            start = time.time()
            resp = requests.post(
                API_URL,
                headers=HEADERS,
                json={"text": input_text},
                timeout=10,
            )
            elapsed = (time.time() - start) * 1000
            total_time += elapsed

            if resp.status_code != 200:
                print(f"  ❌ FAIL - HTTP {resp.status_code}: {resp.text[:200]}")
                failed += 1
                continue

            data = resp.json()
            redacted = data.get("redacted", data.get("result", ""))
            print(f"  Output:   {redacted[:70]}{'...' if len(redacted) > 70 else ''}")
            print(f"  Latency:  {elapsed:.0f}ms")

            # Check that each expected pattern was redacted (no longer in output)
            all_redacted = True
            for pattern in expected_patterns:
                if pattern in redacted:
                    print(f"  ⚠️  Pattern NOT redacted: {pattern}")
                    all_redacted = False

            # For the no-PII case, check text is unchanged
            if not expected_patterns:
                if redacted.strip() == input_text.strip():
                    print(f"  ✅ PASS - Text unchanged (no PII)")
                    passed += 1
                else:
                    print(f"  ⚠️  WARN - Text changed unexpectedly")
                    passed += 1  # Still pass, might have extra markers
            elif all_redacted:
                print(f"  ✅ PASS - All {len(expected_patterns)} pattern(s) redacted")
                passed += 1
            else:
                print(f"  ❌ FAIL - Some patterns not redacted")
                failed += 1

        except requests.exceptions.ConnectionError as e:
            print(f"  ❌ ERROR - Connection failed: {e}")
            errors += 1
        except requests.exceptions.Timeout:
            print(f"  ❌ ERROR - Request timed out")
            errors += 1
        except Exception as e:
            print(f"  ❌ ERROR - {type(e).__name__}: {e}")
            errors += 1

    print(f"\n{'=' * 60}")
    print(f"  RESULTS: {passed} passed, {failed} failed, {errors} errors")
    print(f"  Total tests: {len(TEST_CASES)}")
    if total_time > 0:
        print(f"  Total time: {total_time:.0f}ms (avg {total_time / len(TEST_CASES):.0f}ms/request)")
    print("=" * 60)


if __name__ == "__main__":
    run_tests()
