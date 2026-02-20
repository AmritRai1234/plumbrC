"""
Core ctypes wrapper for libplumbr.so

This module provides the low-level interface to the PlumbrC C library.
"""

import ctypes
import os
from pathlib import Path
from typing import Optional, Dict, List
from ctypes import c_char_p, c_size_t, c_int, c_void_p, POINTER, Structure

from plumbrc.exceptions import LibraryNotFoundError, RedactionError


# Define C structures
class PlumbrConfig(Structure):
    """Configuration for PlumbrC instance."""
    _fields_ = [
        ("pattern_file", c_char_p),
        ("pattern_dir", c_char_p),
        ("compliance", c_char_p),
        ("num_threads", c_int),
        ("quiet", c_int),
    ]


class PlumbrStats(Structure):
    """Statistics from redaction operations."""
    _fields_ = [
        ("lines_processed", c_size_t),
        ("lines_modified", c_size_t),
        ("patterns_matched", c_size_t),
        ("bytes_processed", c_size_t),
        ("elapsed_seconds", ctypes.c_double),
    ]


def _find_library() -> ctypes.CDLL:
    """
    Find and load libplumbr.so from various locations.
    
    Search order:
    1. Bundled library (in package directory)
    2. Build directory (for development)
    3. System library paths (/usr/local/lib, /usr/lib)
    
    Returns:
        Loaded ctypes.CDLL object
        
    Raises:
        LibraryNotFoundError: If library cannot be found
    """
    # Get package directory
    package_dir = Path(__file__).parent
    
    # Search paths in order of preference
    search_paths = [
        # Bundled with package (for binary wheels)
        package_dir / "lib" / "libplumbr.so",
        # Development build
        package_dir.parent.parent / "build" / "lib" / "libplumbr.so",
        # System paths
        Path("/usr/local/lib/libplumbr.so"),
        Path("/usr/lib/libplumbr.so"),
        Path("/usr/lib/x86_64-linux-gnu/libplumbr.so"),
    ]
    
    for path in search_paths:
        if path.exists():
            try:
                return ctypes.CDLL(str(path))
            except OSError as e:
                # Try next path if loading fails
                continue
    
    # If not found, raise error with helpful message
    raise LibraryNotFoundError(
        "Could not find libplumbr.so. "
        "Please ensure PlumbrC is installed correctly. "
        f"Searched: {[str(p) for p in search_paths]}"
    )


# Load library once at module import
_lib = _find_library()

# Set up function signatures
_lib.libplumbr_new.argtypes = [POINTER(PlumbrConfig)]
_lib.libplumbr_new.restype = c_void_p

_lib.libplumbr_redact.argtypes = [c_void_p, c_char_p, c_size_t, POINTER(c_size_t)]
_lib.libplumbr_redact.restype = c_void_p

_lib.libplumbr_free.argtypes = [c_void_p]
_lib.libplumbr_free.restype = None

_lib.libplumbr_free_string.argtypes = [c_void_p]
_lib.libplumbr_free_string.restype = None

_lib.libplumbr_version.argtypes = []
_lib.libplumbr_version.restype = c_char_p

_lib.libplumbr_pattern_count.argtypes = [c_void_p]
_lib.libplumbr_pattern_count.restype = c_size_t

_lib.libplumbr_get_stats.argtypes = [c_void_p]
_lib.libplumbr_get_stats.restype = PlumbrStats

_lib.libplumbr_redact_buffer.argtypes = [c_void_p, c_char_p, c_size_t, POINTER(c_size_t)]
_lib.libplumbr_redact_buffer.restype = c_void_p


class Plumbr:
    """
    High-performance log redaction using PlumbrC.
    
    This class provides a Python interface to the PlumbrC C library for
    detecting and redacting secrets from text data.
    
    Example:
        >>> p = Plumbr()
        >>> p.redact("api_key=sk-proj-abc123")
        'api_key=[REDACTED:api_key]'
        
        >>> # Use as context manager
        >>> with Plumbr() as p:
        ...     safe = p.redact("password=secret123")
        ...     print(safe)
        'password=[REDACTED:password]'
    """
    
    def __init__(
        self,
        pattern_file: Optional[str] = None,
        pattern_dir: Optional[str] = None,
        compliance: Optional[list] = None,
        num_threads: int = 0,
        quiet: bool = True
    ):
        """
        Initialize PlumbrC redactor.
        
        Args:
            pattern_file: Path to custom pattern file (optional)
            pattern_dir: Path to directory containing pattern files (optional)
            compliance: List of compliance profiles, e.g. ['hipaa', 'pci']
                        Available: hipaa, pci, gdpr, soc2, all
            num_threads: Number of worker threads (0 = auto-detect)
            quiet: Suppress statistics output
            
        Raises:
            RuntimeError: If initialization fails
        """
        config = None
        comp_str = None
        if compliance:
            comp_str = ','.join(compliance) if isinstance(compliance, list) else compliance
        
        if pattern_file or pattern_dir or comp_str or num_threads or not quiet:
            config = PlumbrConfig()
            config.pattern_file = pattern_file.encode() if pattern_file else None
            config.pattern_dir = pattern_dir.encode() if pattern_dir else None
            config.compliance = comp_str.encode() if comp_str else None
            config.num_threads = num_threads
            config.quiet = 1 if quiet else 0
        
        self._handle = _lib.libplumbr_new(ctypes.byref(config) if config else None)
        if not self._handle:
            raise RuntimeError("Failed to create Plumbr instance")
    
    def redact(self, text: str) -> str:
        """
        Redact secrets from text.
        
        Args:
            text: Input text to redact
            
        Returns:
            Redacted text with secrets replaced by [REDACTED:type] tags
            
        Raises:
            RedactionError: If redaction fails
        """
        if not text:
            return text
            
        input_bytes = text.encode('utf-8')
        out_len = c_size_t()
        
        result_ptr = _lib.libplumbr_redact(
            self._handle,
            input_bytes,
            len(input_bytes),
            ctypes.byref(out_len)
        )
        
        if not result_ptr:
            raise RedactionError("Redaction operation failed")
        
        try:
            # Copy result and decode
            result = ctypes.string_at(result_ptr, out_len.value).decode('utf-8')
        finally:
            # Free the C string
            _lib.libplumbr_free_string(result_ptr)
        
        return result
    
    def redact_lines(self, lines: List[str]) -> List[str]:
        """
        Redact multiple lines of text (uses bulk buffer API).
        
        Sends all lines to C in a single FFI call, avoiding per-line
        overhead. ~10-50x faster than calling redact() in a loop.
        
        Args:
            lines: List of text lines to redact
            
        Returns:
            List of redacted lines
        """
        if not lines:
            return []
        return self.redact_bulk('\n'.join(lines)).split('\n')
    
    def redact_bulk(self, text: str) -> str:
        """
        Redact a multi-line string in a single FFI call.
        
        This is the fastest way to process many lines from Python.
        Lines are separated by newlines in both input and output.
        
        Args:
            text: Newline-separated input text
            
        Returns:
            Newline-separated redacted text
            
        Raises:
            RedactionError: If redaction fails
        """
        if not text:
            return text
        
        input_bytes = text.encode('utf-8')
        out_len = c_size_t()
        
        result_ptr = _lib.libplumbr_redact_buffer(
            self._handle,
            input_bytes,
            len(input_bytes),
            ctypes.byref(out_len)
        )
        
        if not result_ptr:
            raise RedactionError("Bulk redaction failed")
        
        try:
            result = ctypes.string_at(result_ptr, out_len.value).decode('utf-8')
        finally:
            _lib.libplumbr_free_string(result_ptr)
        
        return result
    
    @property
    def pattern_count(self) -> int:
        """Get number of loaded patterns."""
        return _lib.libplumbr_pattern_count(self._handle)
    
    @property
    def stats(self) -> Dict[str, int]:
        """
        Get processing statistics.
        
        Returns:
            Dictionary with keys: lines_processed, lines_modified, patterns_matched
        """
        s = _lib.libplumbr_get_stats(self._handle)
        return {
            "lines_processed": s.lines_processed,
            "lines_modified": s.lines_modified,
            "patterns_matched": s.patterns_matched,
            "bytes_processed": s.bytes_processed,
            "elapsed_seconds": s.elapsed_seconds,
        }
    
    @staticmethod
    def version() -> str:
        """Get PlumbrC library version."""
        return _lib.libplumbr_version().decode()
    
    def __enter__(self):
        """Context manager entry."""
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit."""
        self.close()
        return False
    
    def close(self):
        """Explicitly free resources."""
        if hasattr(self, '_handle') and self._handle:
            _lib.libplumbr_free(self._handle)
            self._handle = None
    
    def __del__(self):
        """Cleanup on garbage collection."""
        self.close()
    
    def __repr__(self):
        """String representation."""
        return f"Plumbr(patterns={self.pattern_count})"
