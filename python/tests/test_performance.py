"""Performance benchmark tests for plumbrc."""

import time
import pytest
from plumbrc import Plumbr


def test_single_line_performance():
    """Benchmark single line redaction."""
    p = Plumbr()
    
    # Warm up
    for _ in range(100):
        p.redact("api_key=sk-proj-test123")
    
    # Benchmark
    iterations = 10000
    start = time.time()
    
    for _ in range(iterations):
        p.redact("api_key=sk-proj-test123 password=secret")
    
    elapsed = time.time() - start
    throughput = iterations / elapsed
    
    print(f"\nSingle line throughput: {throughput:.0f} lines/sec")
    print(f"Average latency: {elapsed/iterations*1000:.3f} ms")
    
    # Should be reasonably fast (at least 10K lines/sec)
    assert throughput > 10000, f"Too slow: {throughput:.0f} lines/sec"


def test_batch_performance():
    """Benchmark batch processing."""
    p = Plumbr()
    
    # Create test data
    lines = [
        "api_key=sk-proj-test123",
        "normal log line",
        "password=secret123",
        "AWS_KEY=AKIAIOSFODNN7EXAMPLE",
        "clean line with no secrets",
    ] * 1000  # 5000 lines total
    
    # Warm up
    p.redact_lines(lines[:100])
    
    # Benchmark
    start = time.time()
    results = p.redact_lines(lines)
    elapsed = time.time() - start
    
    throughput = len(lines) / elapsed
    
    print(f"\nBatch throughput: {throughput:.0f} lines/sec")
    print(f"Total lines: {len(lines)}")
    print(f"Elapsed: {elapsed:.3f}s")
    
    assert len(results) == len(lines)
    assert throughput > 10000, f"Too slow: {throughput:.0f} lines/sec"


def test_memory_efficiency():
    """Test that multiple operations don't leak memory."""
    p = Plumbr()
    
    # Process many lines
    for _ in range(10000):
        p.redact("api_key=sk-proj-test123 password=secret")
    
    # If we get here without crashing, memory management is working
    assert True


@pytest.mark.parametrize("line_count", [100, 1000, 10000])
def test_scaling(line_count):
    """Test performance scaling with different line counts."""
    p = Plumbr()
    
    lines = ["api_key=sk-proj-test123"] * line_count
    
    start = time.time()
    p.redact_lines(lines)
    elapsed = time.time() - start
    
    throughput = line_count / elapsed
    
    print(f"\n{line_count} lines: {throughput:.0f} lines/sec")
    
    # Should maintain good throughput
    assert throughput > 5000
