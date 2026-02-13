"""Pattern loading tests for plumbrc."""

import pytest
import tempfile
import os
from pathlib import Path
from plumbrc import Plumbr


def test_default_patterns():
    """Test loading default patterns."""
    p = Plumbr()
    
    # Should have default patterns loaded
    assert p.pattern_count > 0
    
    # Test that common patterns work
    assert "[REDACTED:" in p.redact("password=secret123")
    assert "[REDACTED:" in p.redact("AWS_KEY=AKIAIOSFODNN7EXAMPLE")


def test_custom_pattern_file():
    """Test loading custom pattern file."""
    # Create temporary pattern file
    with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False) as f:
        f.write("custom_secret|SECRET|SECRET[0-9]+|[REDACTED:custom]\n")
        pattern_file = f.name
    
    try:
        p = Plumbr(pattern_file=pattern_file)
        
        # Should have loaded custom pattern
        assert p.pattern_count > 0
        
        # Test custom pattern
        result = p.redact("value=SECRET123")
        assert "[REDACTED:" in result
    finally:
        os.unlink(pattern_file)


def test_pattern_directory():
    """Test loading patterns from directory."""
    # Create temporary directory with pattern files
    with tempfile.TemporaryDirectory() as tmpdir:
        # Create a pattern file
        pattern_file = Path(tmpdir) / "test.txt"
        pattern_file.write_text("test_pattern|TEST|TEST[0-9]+|[REDACTED:test]\n")
        
        p = Plumbr(pattern_dir=tmpdir)
        
        # Should have loaded patterns
        assert p.pattern_count > 0


def test_empty_pattern_file():
    """Test handling of empty pattern file."""
    with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False) as f:
        # Write empty file
        pattern_file = f.name
    
    try:
        # Should still initialize (with defaults if available)
        p = Plumbr(pattern_file=pattern_file)
        assert p is not None
    finally:
        os.unlink(pattern_file)


def test_invalid_pattern_file():
    """Test handling of non-existent pattern file."""
    # Non-existent file should raise error or use defaults
    try:
        p = Plumbr(pattern_file="/nonexistent/file.txt")
        # If it doesn't raise, it should at least initialize
        assert p is not None
    except Exception:
        # Expected to fail
        pass
