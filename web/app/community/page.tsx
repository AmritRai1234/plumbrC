"use client";

import { useState } from "react";
import Link from "next/link";
import {
    ArrowRight,
    ExternalLink,
    Terminal,
    Users,
    GitPullRequest,
    MessageCircle,
    BookOpen,
    Shield,
    Heart,
    Star,
    Code2,
    Bug,
    Lightbulb,
    Github,
    Menu,
    X,
} from "lucide-react";

const ways = [
    {
        icon: <Bug size={18} />,
        title: "Report Bugs",
        desc: "Found a bug? Open an issue with steps to reproduce.",
        link: "https://github.com/AmritRai1234/plumbrC/issues/new?template=bug_report.md",
        cta: "Report Bug",
    },
    {
        icon: <Lightbulb size={18} />,
        title: "Request Features",
        desc: "Have an idea? We'd love to hear it.",
        link: "https://github.com/AmritRai1234/plumbrC/issues/new?template=feature_request.md",
        cta: "Request Feature",
    },
    {
        icon: <GitPullRequest size={18} />,
        title: "Submit PRs",
        desc: "Code contributions are always welcome. Check the contributing guide.",
        link: "https://github.com/AmritRai1234/plumbrC/blob/main/.github/CONTRIBUTING.md",
        cta: "Contributing Guide",
    },
    {
        icon: <BookOpen size={18} />,
        title: "Improve Docs",
        desc: "Help us improve documentation, examples, and tutorials.",
        link: "https://github.com/AmritRai1234/plumbrC",
        cta: "View Docs",
    },
    {
        icon: <Star size={18} />,
        title: "Star the Repo",
        desc: "Show your support by starring the repo on GitHub.",
        link: "https://github.com/AmritRai1234/plumbrC",
        cta: "Star on GitHub",
    },
    {
        icon: <MessageCircle size={18} />,
        title: "Join Discussions",
        desc: "Ask questions, share ideas, and connect with other users.",
        link: "https://github.com/AmritRai1234/plumbrC/discussions",
        cta: "Join Discussion",
    },
];

const guidelines = [
    {
        icon: <Heart size={18} />,
        title: "Be Respectful",
        desc: "Treat everyone with respect. We're all here to learn and build together.",
    },
    {
        icon: <Users size={18} />,
        title: "Be Inclusive",
        desc: "We welcome everyone regardless of experience level, background, or identity.",
    },
    {
        icon: <Shield size={18} />,
        title: "Be Constructive",
        desc: "Focus on helpful feedback. Critique code, not people.",
    },
    {
        icon: <Code2 size={18} />,
        title: "Be Collaborative",
        desc: "Share knowledge, help others, and build on each other's work.",
    },
];

export default function CommunityPage() {
    const [mobileMenuOpen, setMobileMenuOpen] = useState(false);

    return (
        <main className="min-h-screen">
            {/* Nav */}
            <nav className="fixed top-0 left-0 right-0 z-50 nav-bar">
                <div className="max-w-5xl mx-auto px-4 sm:px-6 h-14 flex items-center justify-between">
                    <Link href="/" className="flex items-center gap-2.5">
                        <div
                            className="w-7 h-7 rounded-md flex items-center justify-center"
                            style={{ background: "#e8e8e8" }}
                        >
                            <span className="text-xs font-bold" style={{ color: "#141414" }}>
                                P
                            </span>
                        </div>
                        <span className="font-semibold text-[15px]">PlumbrC</span>
                    </Link>
                    <div className="flex items-center gap-1">
                        <Link
                            href="/"
                            className="text-[13px] text-[var(--color-text-secondary)] hover:text-[var(--color-text)] transition-colors px-3 py-1.5 rounded-md hover:bg-[var(--color-bg-elevated)] hidden sm:block"
                        >
                            Home
                        </Link>
                        <Link
                            href="/community"
                            className="text-[13px] text-[var(--color-text)] transition-colors px-3 py-1.5 rounded-md bg-[var(--color-bg-elevated)] hidden sm:block"
                        >
                            Community
                        </Link>
                        <Link
                            href="/developers"
                            className="text-[13px] text-[var(--color-text-secondary)] hover:text-[var(--color-text)] transition-colors px-3 py-1.5 rounded-md hover:bg-[var(--color-bg-elevated)] hidden sm:block"
                        >
                            Developers
                        </Link>
                        <Link
                            href="/playground"
                            className="text-[13px] font-medium px-4 py-1.5 rounded-md transition-opacity hover:opacity-90 ml-2 hidden sm:inline-flex"
                            style={{ background: "#e8e8e8", color: "#141414" }}
                        >
                            Playground
                        </Link>
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
                            <Link href="/community" onClick={() => setMobileMenuOpen(false)} className="block text-[14px] text-[var(--color-text)] py-2 px-3 rounded-lg bg-[var(--color-bg-elevated)]">Community</Link>
                            <Link href="/developers" onClick={() => setMobileMenuOpen(false)} className="block text-[14px] text-[var(--color-text-secondary)] hover:text-[var(--color-text)] py-2 px-3 rounded-lg hover:bg-[var(--color-bg-elevated)] transition-colors">Developers</Link>
                            <div className="border-t border-[var(--color-border)] pt-2 mt-2">
                                <Link href="/playground" onClick={() => setMobileMenuOpen(false)} className="block text-[14px] font-medium py-2 px-3 rounded-lg transition-colors" style={{ color: '#e8e8e8' }}>Playground →</Link>
                            </div>
                        </div>
                    </div>
                )}
            </nav>

            {/* Hero */}
            <section className="pt-32 pb-16 px-6">
                <div className="max-w-2xl mx-auto text-center">
                    <div className="inline-flex items-center gap-2 text-[13px] text-[var(--color-text-secondary)] border border-[var(--color-border)] rounded-full px-4 py-1.5 mb-8">
                        <Users size={14} />
                        Open Source Community
                    </div>

                    <h1 className="text-4xl sm:text-5xl font-bold tracking-tight leading-[1.1] mb-5">
                        Build with us
                    </h1>

                    <p className="text-base text-[var(--color-text-secondary)] max-w-md mx-auto mb-10 leading-relaxed">
                        PlumbrC is open source and community-driven. Join us in building the
                        best log redaction tool.
                    </p>

                    <div className="flex flex-col sm:flex-row items-center justify-center gap-3">
                        <a
                            href="https://github.com/AmritRai1234/plumbrC"
                            target="_blank"
                            rel="noopener"
                            className="font-medium px-6 py-2.5 rounded-lg transition-opacity hover:opacity-90 text-[15px] flex items-center gap-2"
                            style={{ background: "#e8e8e8", color: "#141414" }}
                        >
                            <Github size={16} />
                            View on GitHub
                        </a>
                        <a
                            href="https://github.com/AmritRai1234/plumbrC/discussions"
                            target="_blank"
                            rel="noopener"
                            className="border border-[var(--color-border)] text-[var(--color-text)] font-medium px-6 py-2.5 rounded-lg hover:bg-[var(--color-bg-raised)] transition-colors text-[15px] flex items-center gap-2"
                        >
                            <MessageCircle size={16} />
                            Join Discussions
                        </a>
                    </div>
                </div>
            </section>

            {/* Ways to Contribute */}
            <section className="py-16 px-6 divider">
                <div className="max-w-5xl mx-auto">
                    <div className="text-center mb-12">
                        <h2 className="text-2xl font-semibold mb-3">Ways to Contribute</h2>
                        <p className="text-sm text-[var(--color-text-secondary)]">
                            Every contribution matters, no matter how small.
                        </p>
                    </div>

                    <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-3">
                        {ways.map((w) => (
                            <a
                                key={w.title}
                                href={w.link}
                                target="_blank"
                                rel="noopener"
                                className="card p-5 group hover:border-[var(--color-border-light)] transition-colors"
                            >
                                <div className="w-9 h-9 rounded-lg bg-[var(--color-bg-elevated)] flex items-center justify-center mb-4 text-[var(--color-text-secondary)]">
                                    {w.icon}
                                </div>
                                <h3 className="text-[15px] font-medium text-[var(--color-text)] mb-1.5">
                                    {w.title}
                                </h3>
                                <p className="text-[13px] text-[var(--color-text-secondary)] leading-relaxed mb-4">
                                    {w.desc}
                                </p>
                                <span className="text-[13px] text-[var(--color-text-secondary)] group-hover:text-[var(--color-text)] transition-colors flex items-center gap-1.5">
                                    {w.cta}
                                    <ExternalLink size={11} />
                                </span>
                            </a>
                        ))}
                    </div>
                </div>
            </section>

            {/* Community Guidelines */}
            <section className="py-16 px-6">
                <div className="max-w-3xl mx-auto">
                    <div className="text-center mb-12">
                        <h2 className="text-2xl font-semibold mb-3">
                            Community Guidelines
                        </h2>
                        <p className="text-sm text-[var(--color-text-secondary)]">
                            Our values for a healthy, productive community.
                        </p>
                    </div>

                    <div className="grid grid-cols-1 sm:grid-cols-2 gap-3">
                        {guidelines.map((g) => (
                            <div key={g.title} className="card p-5">
                                <div className="flex items-center gap-3 mb-3">
                                    <div className="text-[var(--color-text-secondary)]">
                                        {g.icon}
                                    </div>
                                    <h3 className="text-[15px] font-medium">{g.title}</h3>
                                </div>
                                <p className="text-[13px] text-[var(--color-text-secondary)] leading-relaxed">
                                    {g.desc}
                                </p>
                            </div>
                        ))}
                    </div>

                    <div className="text-center mt-8">
                        <a
                            href="https://github.com/AmritRai1234/plumbrC/blob/main/.github/CODE_OF_CONDUCT.md"
                            target="_blank"
                            rel="noopener"
                            className="text-[13px] text-[var(--color-text-secondary)] hover:text-[var(--color-text)] transition-colors inline-flex items-center gap-1.5"
                        >
                            Read full Code of Conduct
                            <ExternalLink size={11} />
                        </a>
                    </div>
                </div>
            </section>

            {/* Quick Start */}
            <section className="py-16 px-6 divider">
                <div className="max-w-3xl mx-auto">
                    <div className="text-center mb-12">
                        <h2 className="text-2xl font-semibold mb-3">Get Started</h2>
                        <p className="text-sm text-[var(--color-text-secondary)]">
                            Start using PlumbrC in seconds.
                        </p>
                    </div>

                    <div className="grid grid-cols-1 sm:grid-cols-3 gap-3">
                        <div className="card p-5 text-center">
                            <div className="text-3xl font-bold mb-2">1</div>
                            <h3 className="text-sm font-medium mb-2">Install</h3>
                            <div className="code-block px-3 py-2 text-xs font-mono text-[var(--color-text-secondary)]">
                                pip install plumbrc
                            </div>
                        </div>
                        <div className="card p-5 text-center">
                            <div className="text-3xl font-bold mb-2">2</div>
                            <h3 className="text-sm font-medium mb-2">Import</h3>
                            <div className="code-block px-3 py-2 text-xs font-mono text-[var(--color-text-secondary)]">
                                from plumbrc import Plumbr
                            </div>
                        </div>
                        <div className="card p-5 text-center">
                            <div className="text-3xl font-bold mb-2">3</div>
                            <h3 className="text-sm font-medium mb-2">Redact</h3>
                            <div className="code-block px-3 py-2 text-xs font-mono text-[var(--color-text-secondary)]">
                                p.redact(&quot;secret&quot;)
                            </div>
                        </div>
                    </div>
                </div>
            </section>

            {/* CTA */}
            <section className="py-16 px-6">
                <div className="max-w-md mx-auto text-center">
                    <h2 className="text-2xl font-semibold mb-4">Ready to build?</h2>
                    <p className="text-sm text-[var(--color-text-secondary)] mb-8">
                        Create a developer account to get API keys and start integrating.
                    </p>
                    <div className="flex flex-col sm:flex-row items-center justify-center gap-3">
                        <Link
                            href="/developers"
                            className="font-medium px-6 py-2.5 rounded-lg transition-opacity hover:opacity-90 text-[15px] flex items-center gap-2"
                            style={{ background: "#e8e8e8", color: "#141414" }}
                        >
                            Developer Portal
                            <ArrowRight size={16} />
                        </Link>
                        <Link
                            href="/playground"
                            className="border border-[var(--color-border)] text-[var(--color-text)] font-medium px-6 py-2.5 rounded-lg hover:bg-[var(--color-bg-raised)] transition-colors text-[15px]"
                        >
                            Try Playground
                        </Link>
                    </div>
                </div>
            </section>

            {/* Footer */}
            <footer className="divider py-6 px-6">
                <div className="max-w-5xl mx-auto flex flex-col sm:flex-row items-center justify-between gap-4">
                    <div className="flex items-center gap-2.5">
                        <div
                            className="w-5 h-5 rounded flex items-center justify-center"
                            style={{ background: "#e8e8e8" }}
                        >
                            <span
                                className="text-[9px] font-bold"
                                style={{ color: "#141414" }}
                            >
                                P
                            </span>
                        </div>
                        <span className="text-sm font-medium">PlumbrC</span>
                        <span className="text-[11px] text-[var(--color-text-tertiary)]">
                            Protect your logs.
                        </span>
                    </div>
                    <div className="flex items-center gap-5 text-[13px] text-[var(--color-text-secondary)]">
                        <a
                            href="https://github.com/AmritRai1234/plumbrC"
                            target="_blank"
                            rel="noopener"
                            className="hover:text-[var(--color-text)] transition-colors flex items-center gap-1.5"
                        >
                            GitHub <ExternalLink size={11} />
                        </a>
                        <Link
                            href="/community"
                            className="hover:text-[var(--color-text)] transition-colors"
                        >
                            Community
                        </Link>
                        <Link
                            href="/developers"
                            className="hover:text-[var(--color-text)] transition-colors"
                        >
                            Developers
                        </Link>
                    </div>
                </div>
            </footer>
        </main>
    );
}
