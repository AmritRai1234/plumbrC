"use client";

import { useState, useCallback, useRef } from "react";
import Link from "next/link";
import { Terminal, Copy, Check } from "lucide-react";

const SAMPLE_LOGS = `2024-01-15T12:00:01.234Z INFO  [auth-service] User login successful user=alice password=P@ssw0rd!
2024-01-15T12:00:02.567Z DEBUG [api-gateway] S3 client init: key=AKIAIOSFODNN7EXAMPL3 region=us-east-1
2024-01-15T12:00:03.891Z INFO  [user-service] Registration: email=admin@company.com source=organic
2024-01-15T12:00:04.234Z WARN  [payment-api] Config dump: db_password=mysql://root:SuperSecret123@db.internal:3306/prod
2024-01-15T12:00:05.567Z ERROR [notification-svc] Git clone failed: token=ghp_aBcDeFgHiJkLmNoPqRsTuVwXyZ1234567890
2024-01-15T12:00:06.891Z INFO  [data-pipeline] Processing request from 192.168.1.42 user_agent=curl/7.88
2024-01-15T12:00:07.234Z DEBUG [auth-service] Bearer token: eyJhbGciOiJIUzI1NiJ9.eyJ1c2VyIjoiYWRtaW4ifQ.abcdef
2024-01-15T12:00:08.567Z INFO  [user-service] Contact support at help@startup.io for assistance
2024-01-15T12:00:09.891Z WARN  [api-gateway] Rate limiter: api_key=sk_live_4eC39HqLyjWDarjtT1zdp7dc threshold=1000
2024-01-15T12:00:10.234Z ERROR [payment-api] Stripe webhook failed: secret=whsec_1234567890abcdefghijklmnop`;

export default function Playground() {
    const [input, setInput] = useState(SAMPLE_LOGS);
    const [output, setOutput] = useState("");
    const [loading, setLoading] = useState(false);
    const [copied, setCopied] = useState(false);
    const [stats, setStats] = useState<{
        lines_processed: number;
        lines_modified: number;
        patterns_matched: number;
        processing_time_ms: number;
    } | null>(null);
    const [error, setError] = useState("");
    const timerRef = useRef<ReturnType<typeof setTimeout> | null>(null);

    const redact = useCallback(async (text: string) => {
        if (!text.trim()) {
            setOutput("");
            setStats(null);
            return;
        }
        setLoading(true);
        setError("");
        try {
            const res = await fetch("/api/redact", {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify({ text }),
            });
            if (!res.ok) {
                const err = await res.json();
                setError(err.error || "Redaction failed");
                return;
            }
            const data = await res.json();
            setOutput(data.redacted);
            setStats(data.stats);
        } catch {
            setError("Failed to connect to API");
        } finally {
            setLoading(false);
        }
    }, []);

    const handleInputChange = useCallback(
        (value: string) => {
            setInput(value);
            if (timerRef.current) clearTimeout(timerRef.current);
            timerRef.current = setTimeout(() => redact(value), 300);
        },
        [redact]
    );

    const copyOutput = () => {
        navigator.clipboard.writeText(output);
        setCopied(true);
        setTimeout(() => setCopied(false), 1500);
    };

    const highlightRedacted = (text: string) => {
        if (!text) return null;
        return text.split("\n").map((line, i) => (
            <div key={i} className="min-h-[1.6em]">
                {line.split(/(\[REDACTED:[^\]]+\])/).map((part, j) =>
                    part.startsWith("[REDACTED") ? (
                        <span key={j} className="redacted-tag">{part}</span>
                    ) : (
                        <span key={j}>{part}</span>
                    )
                )}
            </div>
        ));
    };

    return (
        <main className="min-h-screen flex flex-col bg-[var(--color-bg)]">
            {/* Nav */}
            <nav className="nav-bar shrink-0">
                <div className="max-w-[1600px] mx-auto px-4 h-12 flex items-center justify-between">
                    <div className="flex items-center gap-3">
                        <Link href="/" className="flex items-center gap-2">
                            <div className="w-5 h-5 rounded flex items-center justify-center" style={{ background: '#e8e8e8' }}>
                                <span className="text-[8px] font-bold" style={{ color: '#141414' }}>P</span>
                            </div>
                            <span className="font-semibold text-sm">PlumbrC</span>
                        </Link>
                        <span className="text-[var(--color-text-tertiary)]">/</span>
                        <span className="text-sm text-[var(--color-text-secondary)]">Playground</span>
                    </div>

                    <div className="flex items-center gap-4">
                        {stats && (
                            <div className="hidden sm:flex items-center gap-4 text-[11px] font-mono text-[var(--color-text-tertiary)]">
                                <span>
                                    <span className="text-[var(--color-text)]">{stats.lines_modified}</span>/{stats.lines_processed} redacted
                                </span>
                                <span>
                                    <span className="text-[var(--color-text)]">{stats.patterns_matched}</span> matches
                                </span>
                                <span>
                                    <span className="text-[var(--color-text)]">{stats.processing_time_ms.toFixed(1)}</span>ms
                                </span>
                            </div>
                        )}
                        <button
                            onClick={() => redact(input)}
                            disabled={loading}
                            className="disabled:opacity-50 text-xs font-medium px-3.5 py-1.5 rounded-md transition-opacity hover:opacity-90 flex items-center gap-1.5"
                            style={{ background: '#e8e8e8', color: '#141414' }}
                        >
                            {loading ? (
                                <span className="inline-block w-3 h-3 border-2 border-[var(--color-bg)]/30 border-t-[var(--color-bg)] rounded-full animate-spin" />
                            ) : (
                                <Terminal size={12} />
                            )}
                            Redact
                        </button>
                    </div>
                </div>
            </nav>

            {/* Mobile stats */}
            {stats && (
                <div className="sm:hidden border-b border-[var(--color-border)] px-4 py-2 flex items-center justify-center gap-4 text-[11px] font-mono text-[var(--color-text-tertiary)] bg-[var(--color-bg)] shrink-0">
                    <span><span className="text-[var(--color-text)]">{stats.lines_modified}</span>/{stats.lines_processed}</span>
                    <span><span className="text-[var(--color-text)]">{stats.patterns_matched}</span> matches</span>
                    <span><span className="text-[var(--color-text)]">{stats.processing_time_ms.toFixed(1)}</span>ms</span>
                </div>
            )}

            {/* Editor Panes */}
            <div className="flex-1 grid grid-cols-1 lg:grid-cols-2 min-h-0">
                {/* Input */}
                <div className="flex flex-col border-r border-[var(--color-border)] min-h-[40vh] lg:min-h-0">
                    <div className="bg-[var(--color-bg-raised)] px-4 py-2 flex items-center gap-2 border-b border-[var(--color-border)] shrink-0">
                        <div className="flex gap-1.5">
                            <div className="w-2.5 h-2.5 rounded-full bg-[#555]" />
                            <div className="w-2.5 h-2.5 rounded-full bg-[#555]" />
                            <div className="w-2.5 h-2.5 rounded-full bg-[#555]" />
                        </div>
                        <span className="text-[11px] text-[var(--color-text-tertiary)] ml-1.5 font-mono">input</span>
                        <button
                            onClick={() => handleInputChange(SAMPLE_LOGS)}
                            className="ml-auto text-[11px] text-[var(--color-text-tertiary)] hover:text-[var(--color-text-secondary)] transition-colors"
                        >
                            Load sample
                        </button>
                    </div>
                    <textarea
                        value={input}
                        onChange={(e) => handleInputChange(e.target.value)}
                        className="flex-1 bg-[#111111] text-[var(--color-text-secondary)] font-mono text-xs leading-[1.7] p-4 resize-none outline-none w-full"
                        placeholder="Paste your logs here..."
                        spellCheck={false}
                    />
                </div>

                {/* Output */}
                <div className="flex flex-col min-h-[40vh] lg:min-h-0">
                    <div className="bg-[var(--color-bg-raised)] px-4 py-2 flex items-center gap-2 border-b border-[var(--color-border)] shrink-0">
                        <div className="flex gap-1.5">
                            <div className="w-2.5 h-2.5 rounded-full bg-[#555]" />
                            <div className="w-2.5 h-2.5 rounded-full bg-[#555]" />
                            <div className="w-2.5 h-2.5 rounded-full bg-[#555]" />
                        </div>
                        <span className="text-[11px] text-[var(--color-text-tertiary)] ml-1.5 font-mono">output</span>
                        {output && (
                            <button
                                onClick={copyOutput}
                                className="ml-auto text-[11px] text-[var(--color-text-tertiary)] hover:text-[var(--color-text-secondary)] transition-colors flex items-center gap-1"
                            >
                                {copied ? <Check size={11} /> : <Copy size={11} />}
                                {copied ? "Copied" : "Copy"}
                            </button>
                        )}
                    </div>
                    <div className="flex-1 bg-[#111111] p-4 font-mono text-xs leading-[1.7] overflow-auto text-[var(--color-text-secondary)]">
                        {error ? (
                            <div className="text-[var(--color-redacted)] flex items-center gap-2">{error}</div>
                        ) : loading ? (
                            <div className="flex items-center gap-2 text-[var(--color-text-tertiary)]">
                                <span className="inline-block w-3 h-3 border-2 border-[var(--color-text-tertiary)]/30 border-t-[var(--color-text-tertiary)] rounded-full animate-spin" />
                                Processing...
                            </div>
                        ) : output ? (
                            highlightRedacted(output)
                        ) : (
                            <div className="text-[var(--color-text-tertiary)] italic">
                                Redacted output will appear here...
                            </div>
                        )}
                    </div>
                </div>
            </div>
        </main>
    );
}
