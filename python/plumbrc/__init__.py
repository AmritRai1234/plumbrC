"""
PlumbrC - High-Performance Log Redaction for Python

A Python wrapper for the PlumbrC C library, providing blazing-fast
secret detection and redaction for logs and text data.

Example:
    >>> from plumbrc import Plumbr
    >>> p = Plumbr()
    >>> p.redact("api_key=sk-proj-abc123")
    'api_key=[REDACTED:api_key]'
"""

from plumbrc._plumbr import Plumbr
from plumbrc.exceptions import PlumbrError, LibraryNotFoundError, RedactionError

__version__ = "1.0.0"
__all__ = ["Plumbr", "PlumbrError", "LibraryNotFoundError", "RedactionError"]
