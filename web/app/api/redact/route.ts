import { NextRequest, NextResponse } from "next/server";

// The PlumbrC HTTP server runs as a sidecar on port 8081
// It handles redaction with pre-loaded patterns at 60K+ req/sec
const PLUMBR_SERVER = process.env.PLUMBR_SERVER_URL || "http://localhost:8081";

// Max input size: 1MB
const MAX_INPUT_SIZE = 1024 * 1024;

export async function POST(request: NextRequest) {
    try {
        const body = await request.json();
        const { text } = body;

        if (!text || typeof text !== "string") {
            return NextResponse.json(
                { error: "Missing or invalid 'text' field" },
                { status: 400 }
            );
        }

        if (text.length > MAX_INPUT_SIZE) {
            return NextResponse.json(
                { error: `Input too large. Max size: ${MAX_INPUT_SIZE / 1024}KB` },
                { status: 413 }
            );
        }

        // Proxy to the C HTTP server
        const response = await fetch(`${PLUMBR_SERVER}/api/redact`, {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ text }),
            signal: AbortSignal.timeout(5000),
        });

        if (!response.ok) {
            const errorBody = await response.json().catch(() => ({}));
            return NextResponse.json(
                { error: errorBody.error || "Redaction service error" },
                { status: response.status }
            );
        }

        const result = await response.json();
        return NextResponse.json(result);
    } catch (error: unknown) {
        const err = error as { name?: string; message?: string };

        if (err.name === "AbortError" || err.name === "TimeoutError") {
            return NextResponse.json(
                { error: "Processing timed out (5s limit)" },
                { status: 504 }
            );
        }

        // Connection refused — C server not running
        if (err.message?.includes("ECONNREFUSED") || err.message?.includes("fetch failed")) {
            return NextResponse.json(
                {
                    error:
                        "PlumbrC server not running. Start it with: ./build/bin/plumbr-server --port 8081",
                },
                { status: 503 }
            );
        }

        console.error("Redaction error:", err.message);
        return NextResponse.json(
            { error: "Internal server error" },
            { status: 500 }
        );
    }
}
