"""Basic functionality tests for plumbrc package."""

import pytest
from plumbrc import Plumbr, PlumbrError


def test_import():
    """Test that package can be imported."""
    from plumbrc import Plumbr, PlumbrError, LibraryNotFoundError, RedactionError
    assert Plumbr is not None
    assert PlumbrError is not None


def test_initialization():
    """Test Plumbr initialization."""
    p = Plumbr()
    assert p is not None
    assert p.pattern_count > 0


def test_simple_redaction():
    """Test basic redaction functionality."""
    p = Plumbr()
    
    # Test API key redaction
    result = p.redact("api_key=sk-proj-abc123def456")
    assert "[REDACTED:" in result
    assert "sk-proj-abc123def456" not in result


def test_password_redaction():
    """Test password redaction."""
    p = Plumbr()
    
    result = p.redact("password=secret123")
    assert "[REDACTED:" in result
    assert "secret123" not in result


def test_aws_key_redaction():
    """Test AWS key redaction."""
    p = Plumbr()
    
    result = p.redact("AWS_KEY=AKIAIOSFODNN7EXAMPLE")
    assert "[REDACTED:" in result
    assert "AKIAIOSFODNN7EXAMPLE" not in result


def test_github_token_redaction():
    """Test GitHub token redaction."""
    p = Plumbr()
    
    token = "ghp_" + "x" * 36
    result = p.redact(f"token={token}")
    assert "[REDACTED:" in result
    assert token not in result


def test_no_secrets():
    """Test that clean text passes through unchanged."""
    p = Plumbr()
    
    clean_text = "This is a normal log line with no secrets"
    result = p.redact(clean_text)
    assert result == clean_text


def test_empty_string():
    """Test empty string handling."""
    p = Plumbr()
    
    result = p.redact("")
    assert result == ""


def test_pattern_count():
    """Test pattern count property."""
    p = Plumbr()
    
    count = p.pattern_count
    assert isinstance(count, int)
    assert count > 0


def test_stats():
    """Test statistics retrieval."""
    p = Plumbr()
    
    p.redact("api_key=secret123")
    stats = p.stats
    
    assert isinstance(stats, dict)
    assert "lines_processed" in stats
    assert "lines_modified" in stats
    assert "patterns_matched" in stats


def test_version():
    """Test version retrieval."""
    version = Plumbr.version()
    assert isinstance(version, str)
    assert len(version) > 0


def test_context_manager():
    """Test context manager usage."""
    with Plumbr() as p:
        result = p.redact("password=secret")
        assert "[REDACTED:" in result


def test_redact_lines():
    """Test batch line redaction."""
    p = Plumbr()
    
    lines = [
        "password=secret123",
        "normal line",
        "AWS_KEY=AKIAIOSFODNN7EXAMPLE"
    ]
    
    results = p.redact_lines(lines)
    
    assert len(results) == 3
    assert "[REDACTED:" in results[0]
    assert results[1] == "normal line"
    assert "[REDACTED:" in results[2]


def test_multiple_secrets_same_line():
    """Test multiple secrets in same line."""
    p = Plumbr()
    
    result = p.redact("api_key=secret1 password=secret2")
    assert result.count("[REDACTED:") >= 1


def test_repr():
    """Test string representation."""
    p = Plumbr()
    repr_str = repr(p)
    assert "Plumbr" in repr_str
    assert "patterns=" in repr_str
