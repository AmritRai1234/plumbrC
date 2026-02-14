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
    Cpu,
    Package,
    Globe,
    AlertTriangle,
    ChevronRight,
    Menu,
    X,
} from "lucide-react";

export default function DevelopersPage() {
    const [tab, setTab] = useState<"signup" | "login">("signup");
    const [email, setEmail] = useState("");
    const [password, setPassword] = useState("");
    const [name, setName] = useState("");
    const [error, setError] = useState("");
    const [loading, setLoading] = useState(false);
    const [mobileMenuOpen, setMobileMenuOpen] = useState(false);
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
            title: "5M+ Lines/sec",
            desc: "Native C engine with AVX2 SIMD. AMD Ryzen optimized.",
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
            title: "3 Ways to Use",
            desc: "Native C binary, Python package, or REST API.",
        },
        {
            icon: <Lock size={18} />,
            title: "Open Source",
            desc: "MIT licensed. Full source on GitHub. No vendor lock-in.",
        },
    ];

    return (
        <main className="min-h-screen">
            {/* Nav */}
            <nav className="fixed top-0 left-0 right-0 z-50 nav-bar">
                <div className="max-w-5xl mx-auto px-4 sm:px-6 h-14 flex items-center justify-between">
                    <Link href="/" className="flex items-center gap-2.5">
                        <div className="w-7 h-7 rounded-md flex items-center justify-center" style={{ background: "#e8e8e8" }}>
                            <span className="text-xs font-bold" style={{ color: "#141414" }}>P</span>
                        </div>
                        <span className="font-semibold text-[15px]">PlumbrC</span>
                    </Link>
                    <div className="flex items-center gap-1">
                        <Link href="/" className="text-[13px] text-[var(--color-text-secondary)] hover:text-[var(--color-text)] transition-colors px-3 py-1.5 rounded-md hover:bg-[var(--color-bg-elevated)] hidden sm:block">Home</Link>
                        <Link href="/docs" className="text-[13px] text-[var(--color-text-secondary)] hover:text-[var(--color-text)] transition-colors px-3 py-1.5 rounded-md hover:bg-[var(--color-bg-elevated)] hidden sm:block">Docs</Link>
                        <Link href="/developers" className="text-[13px] text-[var(--color-text)] transition-colors px-3 py-1.5 rounded-md bg-[var(--color-bg-elevated)] hidden sm:block">Developers</Link>
                        <Link href="/playground" className="text-[13px] font-medium px-4 py-1.5 rounded-md transition-opacity hover:opacity-90 ml-2 hidden sm:inline-flex" style={{ background: "#e8e8e8", color: "#141414" }}>Playground</Link>
                        <button
                            onClick={() => setMobileMenuOpen(!mobileMenuOpen)}
                            className="sm:hidden p-2 text-[var(--color-text-secondary)] hover:text-[var(--color-text)] transition-colors ml-1"
                            aria-label="Toggle menu"
                        >
                            {mobileMenuOpen ? <X size={20} /> : <Menu size={20} />}
                        </button>
                    </div>
                </div>
                {mobileMenuOpen && (
                    <div className="sm:hidden border-t border-[var(--color-border)] bg-[var(--color-bg)]/95 backdrop-blur-xl">
                        <div className="px-4 py-3 space-y-1">
                            <Link href="/" onClick={() => setMobileMenuOpen(false)} className="block text-[14px] text-[var(--color-text-secondary)] hover:text-[var(--color-text)] py-2 px-3 rounded-lg hover:bg-[var(--color-bg-elevated)] transition-colors">Home</Link>
                            <Link href="/docs" onClick={() => setMobileMenuOpen(false)} className="block text-[14px] text-[var(--color-text-secondary)] hover:text-[var(--color-text)] py-2 px-3 rounded-lg hover:bg-[var(--color-bg-elevated)] transition-colors">Docs</Link>
                            <Link href="/developers" onClick={() => setMobileMenuOpen(false)} className="block text-[14px] text-[var(--color-text)] py-2 px-3 rounded-lg bg-[var(--color-bg-elevated)]">Developers</Link>
                            <div className="border-t border-[var(--color-border)] pt-2 mt-2">
                                <Link href="/playground" onClick={() => setMobileMenuOpen(false)} className="block text-[14px] font-medium py-2 px-3 rounded-lg transition-colors" style={{ color: '#e8e8e8' }}>Playground →</Link>
                            </div>
                        </div>
                    </div>
                )}
            </nav>

            <div className="pt-24 sm:pt-32 pb-14 sm:pb-20 px-4 sm:px-6">
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

                            <div className="grid grid-cols-1 sm:grid-cols-2 gap-2.5">
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

            {/* Performance Guidance Section */}
            <div className="px-4 sm:px-6 pb-14 sm:pb-20">
                <div className="max-w-5xl mx-auto">
                    <div className="text-center mb-12">
                        <h2 className="text-2xl sm:text-3xl font-bold tracking-tight mb-3">Choose Your Integration</h2>
                        <p className="text-[var(--color-text-secondary)] text-[15px] max-w-lg mx-auto">We believe in being honest about performance. Here&apos;s what to expect from each option.</p>
                    </div>

                    <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
                        {/* Native C */}
                        <div className="card p-6 relative overflow-hidden">
                            <div className="absolute top-3 right-3">
                                <span className="text-[10px] font-bold px-2 py-0.5 rounded-full" style={{ background: '#e8e8e8', color: '#141414' }}>RECOMMENDED</span>
                            </div>
                            <div className="flex items-center gap-2.5 mb-4">
                                <div className="w-9 h-9 rounded-lg flex items-center justify-center" style={{ background: 'rgba(232,232,232,0.1)', border: '1px solid var(--color-border)' }}>
                                    <Cpu size={18} />
                                </div>
                                <div>
                                    <h3 className="text-[15px] font-semibold">Native C Binary</h3>
                                    <span className="text-[11px] text-[var(--color-text-tertiary)]">Maximum performance</span>
                                </div>
                            </div>
                            <div className="text-2xl font-bold mb-1" style={{ color: '#e8e8e8' }}>5M+ <span className="text-sm font-normal text-[var(--color-text-secondary)]">lines/sec</span></div>
                            <p className="text-[12px] text-[var(--color-text-tertiary)] mb-4">364 MB/s throughput</p>
                            <ul className="space-y-2 mb-5">
                                <li className="text-[12px] text-[var(--color-text-secondary)] flex items-start gap-2"><ChevronRight size={12} className="mt-0.5 shrink-0" />Pure C with zero overhead</li>
                                <li className="text-[12px] text-[var(--color-text-secondary)] flex items-start gap-2"><ChevronRight size={12} className="mt-0.5 shrink-0" />AVX2 SIMD &amp; SSE 4.2 optimized</li>
                                <li className="text-[12px] text-[var(--color-text-secondary)] flex items-start gap-2"><ChevronRight size={12} className="mt-0.5 shrink-0" />Tuned for AMD Ryzen processors</li>
                                <li className="text-[12px] text-[var(--color-text-secondary)] flex items-start gap-2"><ChevronRight size={12} className="mt-0.5 shrink-0" />Multi-threaded pipeline</li>
                            </ul>
                            <div className="code-block p-3">
                                <pre className="text-[11px] font-mono text-[var(--color-text-secondary)]">{`echo "logs" | ./plumbr -j0`}</pre>
                            </div>
                        </div>

                        {/* Python Package */}
                        <div className="card p-6">
                            <div className="flex items-center gap-2.5 mb-4">
                                <div className="w-9 h-9 rounded-lg flex items-center justify-center" style={{ background: 'rgba(232,232,232,0.06)', border: '1px solid var(--color-border)' }}>
                                    <Package size={18} />
                                </div>
                                <div>
                                    <h3 className="text-[15px] font-semibold">Python Package</h3>
                                    <span className="text-[11px] text-[var(--color-text-tertiary)]">Best of both worlds</span>
                                </div>
                            </div>
                            <div className="text-2xl font-bold mb-1" style={{ color: '#e8e8e8' }}>83K <span className="text-sm font-normal text-[var(--color-text-secondary)]">lines/sec</span></div>
                            <p className="text-[12px] text-[var(--color-text-tertiary)] mb-4">C engine via ctypes FFI</p>
                            <ul className="space-y-2 mb-5">
                                <li className="text-[12px] text-[var(--color-text-secondary)] flex items-start gap-2"><ChevronRight size={12} className="mt-0.5 shrink-0" />pip install plumbrc</li>
                                <li className="text-[12px] text-[var(--color-text-secondary)] flex items-start gap-2"><ChevronRight size={12} className="mt-0.5 shrink-0" />Same C engine under the hood</li>
                                <li className="text-[12px] text-[var(--color-text-secondary)] flex items-start gap-2"><ChevronRight size={12} className="mt-0.5 shrink-0" />No network latency</li>
                                <li className="text-[12px] text-[var(--color-text-secondary)] flex items-start gap-2"><ChevronRight size={12} className="mt-0.5 shrink-0" />Great for CI/CD pipelines</li>
                            </ul>
                            <div className="code-block p-3">
                                <pre className="text-[11px] font-mono text-[var(--color-text-secondary)]">{`from plumbrc import Plumbr
r = Plumbr()
r.redact("AKIA...")`}</pre>
                            </div>
                        </div>

                        {/* REST API */}
                        <div className="card p-6">
                            <div className="flex items-center gap-2.5 mb-4">
                                <div className="w-9 h-9 rounded-lg flex items-center justify-center" style={{ background: 'rgba(232,232,232,0.06)', border: '1px solid var(--color-border)' }}>
                                    <Globe size={18} />
                                </div>
                                <div>
                                    <h3 className="text-[15px] font-semibold">REST API</h3>
                                    <span className="text-[11px] text-[var(--color-text-tertiary)]">Easy integration</span>
                                </div>
                            </div>
                            <div className="text-2xl font-bold mb-1" style={{ color: '#e8e8e8' }}>~0.1ms <span className="text-sm font-normal text-[var(--color-text-secondary)]">processing</span></div>
                            <p className="text-[12px] text-[var(--color-text-tertiary)] mb-4">Network latency is the bottleneck</p>
                            <ul className="space-y-2 mb-5">
                                <li className="text-[12px] text-[var(--color-text-secondary)] flex items-start gap-2"><ChevronRight size={12} className="mt-0.5 shrink-0" />Language-agnostic HTTP calls</li>
                                <li className="text-[12px] text-[var(--color-text-secondary)] flex items-start gap-2"><ChevronRight size={12} className="mt-0.5 shrink-0" />Best for testing &amp; prototyping</li>
                                <li className="text-[12px] text-[var(--color-text-secondary)] flex items-start gap-2"><ChevronRight size={12} className="mt-0.5 shrink-0" />Use batch endpoint for throughput</li>
                                <li className="text-[12px] text-[var(--color-text-secondary)] flex items-start gap-2"><ChevronRight size={12} className="mt-0.5 shrink-0" />Sub-ms server processing time</li>
                            </ul>
                            <div className="code-block p-3">
                                <pre className="text-[11px] font-mono text-[var(--color-text-secondary)]">{`curl -X POST /api/redact \\
  -d '{"text": "..."}'`}</pre>
                            </div>
                        </div>
                    </div>

                    {/* Honest note */}
                    <div className="mt-6 card p-5 flex items-start gap-3">
                        <AlertTriangle size={18} className="text-[var(--color-text-secondary)] shrink-0 mt-0.5" />
                        <div>
                            <p className="text-[13px] font-medium mb-1">A note on performance</p>
                            <p className="text-[12px] text-[var(--color-text-secondary)] leading-relaxed">
                                The REST API processes each request in under 0.1ms on the server, but network round-trip latency (~100-800ms depending on location) dominates the total time.
                                For production workloads, we strongly recommend the <strong>native C binary</strong> or the <strong>Python package</strong> — both run locally with zero network overhead.
                                The C engine is specifically optimized for AMD Ryzen processors using AVX2 and SSE 4.2 SIMD instructions, but works on any x86-64 CPU.
                                The REST API is ideal for testing, prototyping, and low-volume use cases where convenience matters more than throughput.
                            </p>
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
