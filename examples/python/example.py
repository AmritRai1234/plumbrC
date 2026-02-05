#!/usr/bin/env python3
"""
PlumbrC Python Wrapper Example

This demonstrates calling libplumbr.so from Python using ctypes.

Usage:
    python3 example.py

Make sure to build the shared library first:
    cd ../.. && make shared
"""

import ctypes
import os
from ctypes import c_char_p, c_size_t, c_int, c_void_p, POINTER, Structure

# Find the library
lib_paths = [
    "../../build/lib/libplumbr.so",
    "/usr/local/lib/libplumbr.so",
    "/usr/lib/libplumbr.so",
]

lib = None
for path in lib_paths:
    if os.path.exists(path):
        lib = ctypes.CDLL(path)
        break

if not lib:
    print("Error: libplumbr.so not found. Build with: make shared")
    exit(1)

# Define types
class PlumbrConfig(Structure):
    _fields_ = [
        ("pattern_file", c_char_p),
        ("pattern_dir", c_char_p),
        ("num_threads", c_int),
        ("quiet", c_int),
    ]

class PlumbrStats(Structure):
    _fields_ = [
        ("lines_processed", c_size_t),
        ("lines_modified", c_size_t),
        ("patterns_matched", c_size_t),
        ("bytes_processed", c_size_t),
        ("elapsed_seconds", ctypes.c_double),
    ]

# Set up function signatures
lib.plumbr_new.argtypes = [POINTER(PlumbrConfig)]
lib.plumbr_new.restype = c_void_p

lib.plumbr_redact.argtypes = [c_void_p, c_char_p, c_size_t, POINTER(c_size_t)]
lib.plumbr_redact.restype = c_void_p  # Returns char* that we need to free

lib.plumbr_free.argtypes = [c_void_p]
lib.plumbr_free.restype = None

lib.plumbr_version.argtypes = []
lib.plumbr_version.restype = c_char_p

lib.plumbr_pattern_count.argtypes = [c_void_p]
lib.plumbr_pattern_count.restype = c_size_t

lib.plumbr_get_stats.argtypes = [c_void_p]
lib.plumbr_get_stats.restype = PlumbrStats


class Plumbr:
    """Python wrapper for libplumbr"""
    
    def __init__(self, pattern_file=None, pattern_dir=None):
        config = None
        if pattern_file or pattern_dir:
            config = PlumbrConfig()
            config.pattern_file = pattern_file.encode() if pattern_file else None
            config.pattern_dir = pattern_dir.encode() if pattern_dir else None
            config.num_threads = 0
            config.quiet = 1
        
        self._handle = lib.plumbr_new(ctypes.byref(config) if config else None)
        if not self._handle:
            raise RuntimeError("Failed to create Plumbr instance")
    
    def redact(self, text: str) -> str:
        """Redact secrets from text"""
        input_bytes = text.encode('utf-8')
        out_len = c_size_t()
        
        result_ptr = lib.plumbr_redact(
            self._handle, 
            input_bytes, 
            len(input_bytes),
            ctypes.byref(out_len)
        )
        
        if not result_ptr:
            return text
        
        # Copy result and free the C string
        result = ctypes.string_at(result_ptr, out_len.value).decode('utf-8')
        ctypes.CDLL(None).free(result_ptr)
        
        return result
    
    @property
    def pattern_count(self) -> int:
        return lib.plumbr_pattern_count(self._handle)
    
    @property
    def stats(self) -> dict:
        s = lib.plumbr_get_stats(self._handle)
        return {
            "lines_processed": s.lines_processed,
            "lines_modified": s.lines_modified,
            "patterns_matched": s.patterns_matched,
        }
    
    def __del__(self):
        if hasattr(self, '_handle') and self._handle:
            lib.plumbr_free(self._handle)


def main():
    print(f"PlumbrC Python Example")
    print(f"Version: {lib.plumbr_version().decode()}\n")
    
    # Create instance
    p = Plumbr()
    print(f"Loaded {p.pattern_count} patterns\n")
    
    # Test lines
    test_lines = [
        "User login with api_key=sk-proj-abc123def456xyz789",
        "AWS access: AKIAIOSFODNN7EXAMPLE",
        "Database: postgres://user:password123@localhost:5432/db",
        "Normal log line with no secrets",
        "GitHub token: ghp_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
    ]
    
    print("Redacting lines:")
    print("=" * 60)
    
    for line in test_lines:
        redacted = p.redact(line)
        print(f"Input:  {line}")
        print(f"Output: {redacted}\n")
    
    print("=" * 60)
    print(f"Stats: {p.stats}")


if __name__ == "__main__":
    main()
