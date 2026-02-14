"""
PlumbrC gRPC Client

High-performance streaming redaction via gRPC.

Usage:
    from plumbrc.grpc_client import PlumbrGRPC

    client = PlumbrGRPC("localhost:50051")

    # Single line
    result = client.redact("user@email.com logged in")

    # Batch
    results = client.redact_batch(["line1", "line2", "line3"])

    # Streaming (maximum throughput)
    results = client.redact_stream(["line1", "line2", ...])
"""

from __future__ import annotations

from typing import Iterator

import grpc

# Generated protobuf stubs
from plumbrc.proto import plumbr_pb2, plumbr_pb2_grpc


class PlumbrGRPC:
    """gRPC client for PlumbrC redaction service."""

    def __init__(
        self,
        host: str = "localhost:50051",
        *,
        secure: bool = False,
        timeout: float = 30.0,
    ):
        self.host = host
        self.timeout = timeout

        if secure:
            self.channel = grpc.secure_channel(host, grpc.ssl_channel_credentials())
        else:
            self.channel = grpc.insecure_channel(
                host,
                options=[
                    ("grpc.max_send_message_length", 2 * 1024 * 1024),
                    ("grpc.max_receive_message_length", 2 * 1024 * 1024),
                    ("grpc.keepalive_time_ms", 30000),
                    ("grpc.keepalive_timeout_ms", 10000),
                ],
            )

        self.stub = plumbr_pb2_grpc.PlumbrServiceStub(self.channel)

    def redact(self, text: str) -> str:
        """Redact a single line."""
        req = plumbr_pb2.RedactRequest(text=text)
        resp = self.stub.Redact(req, timeout=self.timeout)
        return resp.redacted

    def redact_batch(self, texts: list[str]) -> list[str]:
        """Redact multiple lines in one call."""
        req = plumbr_pb2.RedactBatchRequest(texts=texts)
        resp = self.stub.RedactBatch(req, timeout=self.timeout)
        return [r.redacted for r in resp.results]

    def redact_stream(self, texts: list[str]) -> list[str]:
        """
        Bidirectional streaming redaction — maximum throughput.

        Streams lines to the server and collects redacted lines back.
        """

        def request_iterator() -> Iterator[plumbr_pb2.RedactRequest]:
            for text in texts:
                yield plumbr_pb2.RedactRequest(text=text)

        responses = self.stub.RedactStream(
            request_iterator(), timeout=self.timeout
        )
        return [resp.redacted for resp in responses]

    def health(self) -> dict:
        """Check server health."""
        req = plumbr_pb2.HealthRequest()
        resp = self.stub.Health(req, timeout=5.0)
        return {
            "status": resp.status,
            "version": resp.version,
            "patterns_loaded": resp.patterns_loaded,
            "uptime_seconds": resp.uptime_seconds,
        }

    def close(self):
        """Close the gRPC channel."""
        self.channel.close()

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()
