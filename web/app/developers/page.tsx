"use client";

import { useState } from "react";
import Link from "next/link";
import { useRouter } from "next/navigation";
import {
    ArrowRight,
    ExternalLink,
    Key,
    Zap,
    Shield,
    BarChart3,
    Code2,
    Terminal,
    Lock,
    Rocket,
} from "lucide-react";

export default function DevelopersPage() {
    const [tab, setTab] = useState<"signup" | "login">("signup");
    const [email, setEmail] = useState("");
    const [password, setPassword] = useState("");
    const [name, setName] = useState("");
    const [error, setError] = useState("");
    const [loading, setLoading] = useState(false);
    const router = useRouter();

    async function handleSubmit(e: React.FormEvent) {
        e.preventDefault();
        setError("");
        setLoading(true);

        try {
            const res = await fetch("/api/auth", {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify({
                    action: tab,
                    email,
                    password,
                    ...(tab === "signup" ? { name } : {}),
                }),
            });

            const data = await res.json();
            if (!res.ok) {
                setError(data.error || "Something went wrong");
                return;
            }

            router.push("/developers/dashboard");
        } catch {
            setError("Network error. Please try again.");
        } finally {
            setLoading(false);
        }
    }

    const features = [
        {
            icon: <Key size={18} />,
            title: "API Keys",
            desc: "Generate unique API keys for each of your applications.",
        },
        {
            icon: <Zap size={18} />,
            title: "High Performance",
            desc: "71K+ lines/sec throughput via the C engine.",
        },
        {
            icon: <Shield size={18} />,
            title: "Secure by Default",
            desc: "HTTPS only. Keys never logged. Ephemeral processing.",
        },
        {
            icon: <BarChart3 size={18} />,
            title: "Usage Analytics",
            desc: "Track API calls, lines processed, and more.",
        },
        {
            icon: <Code2 size={18} />,
            title: "SDKs & Docs",
            desc: "Python package, REST API, and comprehensive docs.",
        },
        {
            icon: <Lock size={18} />,
            title: "Rate Limiting",
            desc: "1,000 req/min default. Contact us for higher limits.",
        },
    ];

    return (
        <main className="min-h-screen">
            {/* Nav */}
            <nav className="fixed top-0 left-0 right-0 z-50 nav-bar">
                <div className="max-w-5xl mx-auto px-6 h-14 flex items-center justify-between">
                    <Link href="/" className="flex items-center gap-2.5">
                        <div className="w-7 h-7 rounded-md flex items-center justify-center" style={{ background: "#e8e8e8" }}>
                            <span className="text-xs font-bold" style={{ color: "#141414" }}>P</span>
                        </div>
                        <span className="font-semibold text-[15px]">PlumbrC</span>
                    </Link>
                    <div className="flex items-center gap-1">
                        <Link href="/" className="text-[13px] text-[var(--color-text-secondary)] hover:text-[var(--color-text)] transition-colors px-3 py-1.5 rounded-md hover:bg-[var(--color-bg-elevated)] hidden sm:block">Home</Link>
                        <Link href="/community" className="text-[13px] text-[var(--color-text-secondary)] hover:text-[var(--color-text)] transition-colors px-3 py-1.5 rounded-md hover:bg-[var(--color-bg-elevated)] hidden sm:block">Community</Link>
                        <Link href="/developers" className="text-[13px] text-[var(--color-text)] transition-colors px-3 py-1.5 rounded-md bg-[var(--color-bg-elevated)] hidden sm:block">Developers</Link>
                        <Link href="/playground" className="text-[13px] font-medium px-4 py-1.5 rounded-md transition-opacity hover:opacity-90 ml-2" style={{ background: "#e8e8e8", color: "#141414" }}>Playground</Link>
                    </div>
                </div>
            </nav>

            <div className="pt-32 pb-20 px-6">
                <div className="max-w-5xl mx-auto">
                    <div className="grid grid-cols-1 lg:grid-cols-2 gap-16">

                        {/* Left — Info */}
                        <div>
                            <div className="inline-flex items-center gap-2 text-[13px] text-[var(--color-text-secondary)] border border-[var(--color-border)] rounded-full px-4 py-1.5 mb-8">
                                <Rocket size={14} />
                                Developer Portal
                            </div>

                            <h1 className="text-3xl sm:text-4xl font-bold tracking-tight leading-[1.1] mb-5">
                                Build with the
                                <br />
                                PlumbrC API
                            </h1>

                            <p className="text-base text-[var(--color-text-secondary)] mb-10 leading-relaxed max-w-md">
                                Create an account, register your app, and get API keys to integrate
                                log redaction into your workflow.
                            </p>

                            {/* API Example */}
                            <div className="code-block p-5 mb-8">
                                <div className="flex items-center gap-2 mb-3">
                                    <Terminal size={14} className="text-[var(--color-text-tertiary)]" />
                                    <span className="text-xs font-mono text-[var(--color-text-tertiary)]">Quick Start</span>
                                </div>
                                <pre className="text-xs font-mono leading-relaxed text-[var(--color-text-secondary)] overflow-x-auto">
                                    {`curl -X POST https://plumbr.ca/api/redact \\
  -H "Authorization: Bearer plumbr_live_xxx" \\
  -H "Content-Type: application/json" \\
  -d '{"text": "password=secret123"}'`}
                                </pre>
                            </div>

                            <div className="grid grid-cols-2 gap-2.5">
                                {features.map((f) => (
                                    <div key={f.title} className="card p-4">
                                        <div className="flex items-center gap-2.5 mb-2">
                                            <span className="text-[var(--color-text-secondary)]">{f.icon}</span>
                                            <span className="text-[13px] font-medium">{f.title}</span>
                                        </div>
                                        <p className="text-[11px] text-[var(--color-text-tertiary)] leading-relaxed">{f.desc}</p>
                                    </div>
                                ))}
                            </div>
                        </div>

                        {/* Right — Auth Form */}
                        <div>
                            <div className="card p-6 sm:p-8 max-w-md mx-auto lg:mt-12">
                                {/* Tabs */}
                                <div className="flex gap-1 mb-8 p-1 rounded-lg bg-[var(--color-bg-elevated)]">
                                    <button
                                        onClick={() => { setTab("signup"); setError(""); }}
                                        className={`flex-1 text-[13px] font-medium py-2 rounded-md transition-colors ${tab === "signup"
                                                ? "bg-[var(--color-bg)] text-[var(--color-text)] shadow-sm"
                                                : "text-[var(--color-text-secondary)] hover:text-[var(--color-text)]"
                                            }`}
                                    >
                                        Create Account
                                    </button>
                                    <button
                                        onClick={() => { setTab("login"); setError(""); }}
                                        className={`flex-1 text-[13px] font-medium py-2 rounded-md transition-colors ${tab === "login"
                                                ? "bg-[var(--color-bg)] text-[var(--color-text)] shadow-sm"
                                                : "text-[var(--color-text-secondary)] hover:text-[var(--color-text)]"
                                            }`}
                                    >
                                        Sign In
                                    </button>
                                </div>

                                <form onSubmit={handleSubmit} className="space-y-4">
                                    {tab === "signup" && (
                                        <div>
                                            <label className="block text-[13px] text-[var(--color-text-secondary)] mb-1.5">Name</label>
                                            <input
                                                type="text"
                                                value={name}
                                                onChange={(e) => setName(e.target.value)}
                                                className="w-full px-3.5 py-2.5 rounded-lg border border-[var(--color-border)] bg-[var(--color-bg)] text-sm text-[var(--color-text)] placeholder:text-[var(--color-text-tertiary)] outline-none focus:border-[var(--color-border-light)] transition-colors"
                                                placeholder="Your name"
                                            />
                                        </div>
                                    )}

                                    <div>
                                        <label className="block text-[13px] text-[var(--color-text-secondary)] mb-1.5">Email</label>
                                        <input
                                            type="email"
                                            value={email}
                                            onChange={(e) => setEmail(e.target.value)}
                                            className="w-full px-3.5 py-2.5 rounded-lg border border-[var(--color-border)] bg-[var(--color-bg)] text-sm text-[var(--color-text)] placeholder:text-[var(--color-text-tertiary)] outline-none focus:border-[var(--color-border-light)] transition-colors"
                                            placeholder="you@example.com"
                                            required
                                        />
                                    </div>

                                    <div>
                                        <label className="block text-[13px] text-[var(--color-text-secondary)] mb-1.5">Password</label>
                                        <input
                                            type="password"
                                            value={password}
                                            onChange={(e) => setPassword(e.target.value)}
                                            className="w-full px-3.5 py-2.5 rounded-lg border border-[var(--color-border)] bg-[var(--color-bg)] text-sm text-[var(--color-text)] placeholder:text-[var(--color-text-tertiary)] outline-none focus:border-[var(--color-border-light)] transition-colors"
                                            placeholder="Min. 8 characters"
                                            minLength={8}
                                            required
                                        />
                                    </div>

                                    {error && (
                                        <div className="text-[13px] text-red-400 bg-red-400/10 rounded-lg px-3.5 py-2.5">
                                            {error}
                                        </div>
                                    )}

                                    <button
                                        type="submit"
                                        disabled={loading}
                                        className="w-full font-medium py-2.5 rounded-lg transition-opacity hover:opacity-90 text-[15px] flex items-center justify-center gap-2 disabled:opacity-50"
                                        style={{ background: "#e8e8e8", color: "#141414" }}
                                    >
                                        {loading ? (
                                            <div className="w-4 h-4 border-2 border-[#141414]/30 border-t-[#141414] rounded-full animate-spin" />
                                        ) : (
                                            <>
                                                {tab === "signup" ? "Create Account" : "Sign In"}
                                                <ArrowRight size={16} />
                                            </>
                                        )}
                                    </button>
                                </form>

                                <p className="text-[11px] text-[var(--color-text-tertiary)] text-center mt-6">
                                    By creating an account, you agree to our{" "}
                                    <a href="https://github.com/AmritRai1234/plumbrC/blob/main/.github/CODE_OF_CONDUCT.md" className="underline hover:text-[var(--color-text-secondary)]">
                                        Code of Conduct
                                    </a>
                                </p>
                            </div>
                        </div>
                    </div>
                </div>
            </div>

            {/* Footer */}
            <footer className="divider py-6 px-6">
                <div className="max-w-5xl mx-auto flex flex-col sm:flex-row items-center justify-between gap-4">
                    <div className="flex items-center gap-2.5">
                        <div className="w-5 h-5 rounded flex items-center justify-center" style={{ background: "#e8e8e8" }}>
                            <span className="text-[9px] font-bold" style={{ color: "#141414" }}>P</span>
                        </div>
                        <span className="text-sm font-medium">PlumbrC</span>
                    </div>
                    <div className="flex items-center gap-5 text-[13px] text-[var(--color-text-secondary)]">
                        <a href="https://github.com/AmritRai1234/plumbrC" target="_blank" rel="noopener" className="hover:text-[var(--color-text)] transition-colors flex items-center gap-1.5">
                            GitHub <ExternalLink size={11} />
                        </a>
                        <Link href="/community" className="hover:text-[var(--color-text)] transition-colors">Community</Link>
                        <Link href="/playground" className="hover:text-[var(--color-text)] transition-colors">Playground</Link>
                    </div>
                </div>
            </footer>
        </main>
    );
}
