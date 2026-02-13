"""Custom exceptions for PlumbrC Python wrapper."""


class PlumbrError(Exception):
    """Base exception for all PlumbrC errors."""
    pass


class LibraryNotFoundError(PlumbrError):
    """Raised when libplumbr.so cannot be found."""
    pass


class RedactionError(PlumbrError):
    """Raised when redaction operation fails."""
    pass
