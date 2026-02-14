from google.protobuf.internal import containers as _containers
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Iterable as _Iterable, Mapping as _Mapping
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class RedactRequest(_message.Message):
    __slots__ = ("text",)
    TEXT_FIELD_NUMBER: _ClassVar[int]
    text: str
    def __init__(self, text: _Optional[str] = ...) -> None: ...

class RedactResponse(_message.Message):
    __slots__ = ("redacted", "patterns_matched", "processing_time_ms")
    REDACTED_FIELD_NUMBER: _ClassVar[int]
    PATTERNS_MATCHED_FIELD_NUMBER: _ClassVar[int]
    PROCESSING_TIME_MS_FIELD_NUMBER: _ClassVar[int]
    redacted: str
    patterns_matched: int
    processing_time_ms: float
    def __init__(self, redacted: _Optional[str] = ..., patterns_matched: _Optional[int] = ..., processing_time_ms: _Optional[float] = ...) -> None: ...

class RedactBatchRequest(_message.Message):
    __slots__ = ("texts",)
    TEXTS_FIELD_NUMBER: _ClassVar[int]
    texts: _containers.RepeatedScalarFieldContainer[str]
    def __init__(self, texts: _Optional[_Iterable[str]] = ...) -> None: ...

class RedactBatchResponse(_message.Message):
    __slots__ = ("results", "total_lines", "total_modified", "processing_time_ms")
    RESULTS_FIELD_NUMBER: _ClassVar[int]
    TOTAL_LINES_FIELD_NUMBER: _ClassVar[int]
    TOTAL_MODIFIED_FIELD_NUMBER: _ClassVar[int]
    PROCESSING_TIME_MS_FIELD_NUMBER: _ClassVar[int]
    results: _containers.RepeatedCompositeFieldContainer[RedactResponse]
    total_lines: int
    total_modified: int
    processing_time_ms: float
    def __init__(self, results: _Optional[_Iterable[_Union[RedactResponse, _Mapping]]] = ..., total_lines: _Optional[int] = ..., total_modified: _Optional[int] = ..., processing_time_ms: _Optional[float] = ...) -> None: ...

class HealthRequest(_message.Message):
    __slots__ = ()
    def __init__(self) -> None: ...

class HealthResponse(_message.Message):
    __slots__ = ("status", "version", "patterns_loaded", "uptime_seconds")
    STATUS_FIELD_NUMBER: _ClassVar[int]
    VERSION_FIELD_NUMBER: _ClassVar[int]
    PATTERNS_LOADED_FIELD_NUMBER: _ClassVar[int]
    UPTIME_SECONDS_FIELD_NUMBER: _ClassVar[int]
    status: str
    version: str
    patterns_loaded: int
    uptime_seconds: float
    def __init__(self, status: _Optional[str] = ..., version: _Optional[str] = ..., patterns_loaded: _Optional[int] = ..., uptime_seconds: _Optional[float] = ...) -> None: ...
