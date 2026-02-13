import { NextRequest, NextResponse } from "next/server";
import { spawn } from "child_process";
import path from "path";

// Path to the plumbr binary (built in the parent directory)
const PLUMBR_BIN = path.resolve(process.cwd(), "..", "build", "bin", "plumbr");

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

        const start = performance.now();

        // Use spawn for better control over stdin/stdout
        const result = await new Promise<{ stdout: string; stderr: string }>(
            (resolve, reject) => {
                const child = spawn(PLUMBR_BIN, ["--threads", "1", "--quiet"], {
                    stdio: ["pipe", "pipe", "pipe"],
                });

                let stdout = "";
                let stderr = "";

                child.stdout.on("data", (data: Buffer) => {
                    stdout += data.toString();
                });

                child.stderr.on("data", (data: Buffer) => {
                    stderr += data.toString();
                });

                child.on("close", (code: number | null) => {
                    if (code === 0) {
                        resolve({ stdout, stderr });
                    } else {
                        reject(new Error(`Process exited with code ${code}: ${stderr}`));
                    }
                });

                child.on("error", (err: Error) => {
                    reject(err);
                });

                // Set timeout
                const timer = setTimeout(() => {
                    child.kill("SIGKILL");
                    reject(new Error("timeout"));
                }, 5000);

                child.on("close", () => clearTimeout(timer));

                // Write input and close stdin
                child.stdin.write(text);
                child.stdin.end();
            }
        );

        const elapsed = performance.now() - start;

        // Calculate stats
        const inputLines = text.split("\n");
        const outputLines = result.stdout.replace(/\n$/, "").split("\n");
        const linesProcessed = inputLines.length;
        const linesModified = outputLines.filter(
            (line: string, i: number) => i < inputLines.length && line !== inputLines[i]
        ).length;
        const patternsMatched = (result.stdout.match(/\[REDACTED:[^\]]+\]/g) || []).length;

        return NextResponse.json({
            redacted: result.stdout.replace(/\n$/, ""),
            stats: {
                lines_processed: linesProcessed,
                lines_modified: linesModified,
                patterns_matched: patternsMatched,
                processing_time_ms: elapsed,
            },
        });
    } catch (error: unknown) {
        const err = error as { code?: string; message?: string };

        if (err.code === "ENOENT") {
            return NextResponse.json(
                {
                    error:
                        "PlumbrC binary not found. Run 'make' in the project root to build it.",
                },
                { status: 500 }
            );
        }

        if (err.message === "timeout") {
            return NextResponse.json(
                { error: "Processing timed out (5s limit)" },
                { status: 504 }
            );
        }

        console.error("Redaction error:", err.message);
        return NextResponse.json(
            { error: "Internal server error" },
            { status: 500 }
        );
    }
}
