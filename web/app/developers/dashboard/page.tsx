"use client";

import { useState, useEffect } from "react";
import { useRouter } from "next/navigation";
import Link from "next/link";
import {
    Plus,
    Key,
    Trash2,
    Copy,
    Check,
    RefreshCw,
    Eye,
    EyeOff,
    ExternalLink,
    LogOut,
    ArrowRight,
    Terminal,
    Shield,
    Zap,
} from "lucide-react";

interface App {
    id: string;
    name: string;
    description: string;
    api_key: string;
    api_secret?: string;
    status: string;
    rate_limit: number;
    created_at: string;
}

interface User {
    id: string;
    email: string;
    name: string;
    created_at: string;
}

export default function DashboardPage() {
    const [user, setUser] = useState<User | null>(null);
    const [apps, setApps] = useState<App[]>([]);
    const [loading, setLoading] = useState(true);
    const [showCreate, setShowCreate] = useState(false);
    const [newName, setNewName] = useState("");
    const [newDesc, setNewDesc] = useState("");
    const [creating, setCreating] = useState(false);
    const [copied, setCopied] = useState<string | null>(null);
    const [visibleKeys, setVisibleKeys] = useState<Record<string, boolean>>({});
    const [newAppSecret, setNewAppSecret] = useState<{ id: string; secret: string } | null>(null);
    const router = useRouter();

    useEffect(() => {
        checkAuth();
    }, []);

    async function checkAuth() {
        const res = await fetch("/api/auth");
        if (!res.ok) {
            router.push("/developers");
            return;
        }
        const data = await res.json();
        setUser(data.user);
        fetchApps();
    }

    async function fetchApps() {
        const res = await fetch("/api/apps");
        if (res.ok) {
            const data = await res.json();
            setApps(data.apps);
        }
        setLoading(false);
    }

    async function createApp(e: React.FormEvent) {
        e.preventDefault();
        setCreating(true);
        const res = await fetch("/api/apps", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ action: "create", name: newName, description: newDesc }),
        });
        const data = await res.json();
        if (res.ok) {
            setNewAppSecret({ id: data.app.id, secret: data.app.api_secret });
            setNewName("");
            setNewDesc("");
            setShowCreate(false);
            fetchApps();
        }
        setCreating(false);
    }

    async function deleteApp(appId: string) {
        if (!confirm("Are you sure? This will revoke all API keys for this app.")) return;
        await fetch("/api/apps", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ action: "delete", appId }),
        });
        fetchApps();
        if (newAppSecret?.id === appId) setNewAppSecret(null);
    }

    async function regenerateKeys(appId: string) {
        if (!confirm("This will invalidate your current API keys. Continue?")) return;
        const res = await fetch("/api/apps", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ action: "regenerate", appId }),
        });
        const data = await res.json();
        if (res.ok) {
            setNewAppSecret({ id: appId, secret: data.api_secret });
            fetchApps();
        }
    }

    async function logout() {
        await fetch("/api/auth", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ action: "logout" }),
        });
        router.push("/developers");
    }

    function copyToClipboard(text: string, id: string) {
        navigator.clipboard.writeText(text);
        setCopied(id);
        setTimeout(() => setCopied(null), 2000);
    }

    function maskKey(key: string) {
        return key.slice(0, 16) + "••••••••••••••••";
    }

    if (loading) {
        return (
            <main className="min-h-screen flex items-center justify-center">
                <div className="w-6 h-6 border-2 border-[var(--color-text-tertiary)] border-t-[var(--color-text)] rounded-full animate-spin" />
            </main>
        );
    }

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
                    <div className="flex items-center gap-3">
                        <span className="text-[13px] text-[var(--color-text-secondary)]">{user?.email}</span>
                        <button
                            onClick={logout}
                            className="text-[13px] text-[var(--color-text-secondary)] hover:text-[var(--color-text)] transition-colors flex items-center gap-1.5 px-3 py-1.5 rounded-md hover:bg-[var(--color-bg-elevated)]"
                        >
                            <LogOut size={14} />
                            Sign Out
                        </button>
                    </div>
                </div>
            </nav>

            <div className="pt-24 pb-20 px-6">
                <div className="max-w-3xl mx-auto">
                    {/* Header */}
                    <div className="flex items-center justify-between mb-8">
                        <div>
                            <h1 className="text-2xl font-semibold mb-1">Developer Dashboard</h1>
                            <p className="text-sm text-[var(--color-text-secondary)]">
                                Manage your apps and API keys
                            </p>
                        </div>
                        <button
                            onClick={() => setShowCreate(true)}
                            className="font-medium px-4 py-2 rounded-lg transition-opacity hover:opacity-90 text-[13px] flex items-center gap-2"
                            style={{ background: "#e8e8e8", color: "#141414" }}
                        >
                            <Plus size={14} />
                            Create App
                        </button>
                    </div>

                    {/* New App Secret Banner */}
                    {newAppSecret && (
                        <div className="card p-5 mb-6 border-[var(--color-green)]/30 bg-[var(--color-green)]/5">
                            <div className="flex items-center gap-2 mb-3">
                                <Shield size={16} className="text-[var(--color-green)]" />
                                <span className="text-sm font-medium text-[var(--color-green)]">
                                    Save your API secret — it won&apos;t be shown again!
                                </span>
                            </div>
                            <div className="code-block p-3 flex items-center justify-between">
                                <code className="text-xs font-mono text-[var(--color-text-secondary)] break-all">
                                    {newAppSecret.secret}
                                </code>
                                <button
                                    onClick={() => copyToClipboard(newAppSecret.secret, "secret")}
                                    className="ml-3 text-[var(--color-text-tertiary)] hover:text-[var(--color-text-secondary)] transition-colors flex-shrink-0"
                                >
                                    {copied === "secret" ? <Check size={14} className="text-[var(--color-green)]" /> : <Copy size={14} />}
                                </button>
                            </div>
                            <button
                                onClick={() => setNewAppSecret(null)}
                                className="text-[11px] text-[var(--color-text-tertiary)] mt-3 hover:text-[var(--color-text-secondary)]"
                            >
                                I&apos;ve saved it — dismiss
                            </button>
                        </div>
                    )}

                    {/* Create App Modal */}
                    {showCreate && (
                        <div className="card p-6 mb-6">
                            <h3 className="text-base font-medium mb-4">Create New App</h3>
                            <form onSubmit={createApp} className="space-y-4">
                                <div>
                                    <label className="block text-[13px] text-[var(--color-text-secondary)] mb-1.5">App Name</label>
                                    <input
                                        type="text"
                                        value={newName}
                                        onChange={(e) => setNewName(e.target.value)}
                                        className="w-full px-3.5 py-2.5 rounded-lg border border-[var(--color-border)] bg-[var(--color-bg)] text-sm text-[var(--color-text)] placeholder:text-[var(--color-text-tertiary)] outline-none focus:border-[var(--color-border-light)] transition-colors"
                                        placeholder="My Log Redactor"
                                        required
                                    />
                                </div>
                                <div>
                                    <label className="block text-[13px] text-[var(--color-text-secondary)] mb-1.5">Description (optional)</label>
                                    <input
                                        type="text"
                                        value={newDesc}
                                        onChange={(e) => setNewDesc(e.target.value)}
                                        className="w-full px-3.5 py-2.5 rounded-lg border border-[var(--color-border)] bg-[var(--color-bg)] text-sm text-[var(--color-text)] placeholder:text-[var(--color-text-tertiary)] outline-none focus:border-[var(--color-border-light)] transition-colors"
                                        placeholder="Production log pipeline"
                                    />
                                </div>
                                <div className="flex items-center gap-3">
                                    <button
                                        type="submit"
                                        disabled={creating}
                                        className="font-medium px-4 py-2 rounded-lg text-[13px] transition-opacity hover:opacity-90 disabled:opacity-50"
                                        style={{ background: "#e8e8e8", color: "#141414" }}
                                    >
                                        {creating ? "Creating..." : "Create App"}
                                    </button>
                                    <button
                                        type="button"
                                        onClick={() => setShowCreate(false)}
                                        className="text-[13px] text-[var(--color-text-secondary)] hover:text-[var(--color-text)] transition-colors"
                                    >
                                        Cancel
                                    </button>
                                </div>
                            </form>
                        </div>
                    )}

                    {/* Apps List */}
                    {apps.length === 0 ? (
                        <div className="card p-12 text-center">
                            <Key size={32} className="text-[var(--color-text-tertiary)] mx-auto mb-4" />
                            <h3 className="text-base font-medium mb-2">No apps yet</h3>
                            <p className="text-sm text-[var(--color-text-secondary)] mb-6">
                                Create your first app to get an API key
                            </p>
                            <button
                                onClick={() => setShowCreate(true)}
                                className="font-medium px-5 py-2 rounded-lg transition-opacity hover:opacity-90 text-[13px] inline-flex items-center gap-2"
                                style={{ background: "#e8e8e8", color: "#141414" }}
                            >
                                <Plus size={14} />
                                Create App
                            </button>
                        </div>
                    ) : (
                        <div className="space-y-3">
                            {apps.map((app) => (
                                <div key={app.id} className="card p-5">
                                    <div className="flex items-start justify-between mb-4">
                                        <div>
                                            <div className="flex items-center gap-2.5">
                                                <h3 className="text-[15px] font-medium">{app.name}</h3>
                                                <span className={`text-[10px] px-2 py-0.5 rounded-full font-medium ${app.status === "active"
                                                        ? "bg-[var(--color-green)]/10 text-[var(--color-green)]"
                                                        : "bg-red-400/10 text-red-400"
                                                    }`}>
                                                    {app.status}
                                                </span>
                                            </div>
                                            {app.description && (
                                                <p className="text-[13px] text-[var(--color-text-secondary)] mt-1">{app.description}</p>
                                            )}
                                        </div>
                                        <div className="flex items-center gap-1">
                                            <button
                                                onClick={() => regenerateKeys(app.id)}
                                                className="p-2 rounded-md text-[var(--color-text-tertiary)] hover:text-[var(--color-text-secondary)] hover:bg-[var(--color-bg-elevated)] transition-colors"
                                                title="Regenerate keys"
                                            >
                                                <RefreshCw size={14} />
                                            </button>
                                            <button
                                                onClick={() => deleteApp(app.id)}
                                                className="p-2 rounded-md text-[var(--color-text-tertiary)] hover:text-red-400 hover:bg-red-400/10 transition-colors"
                                                title="Delete app"
                                            >
                                                <Trash2 size={14} />
                                            </button>
                                        </div>
                                    </div>

                                    {/* API Key */}
                                    <div className="space-y-2">
                                        <label className="text-[11px] text-[var(--color-text-tertiary)] uppercase tracking-wider font-medium">API Key</label>
                                        <div className="code-block px-3.5 py-2.5 flex items-center justify-between">
                                            <code className="text-xs font-mono text-[var(--color-text-secondary)]">
                                                {visibleKeys[app.id] ? app.api_key : maskKey(app.api_key)}
                                            </code>
                                            <div className="flex items-center gap-1 ml-3">
                                                <button
                                                    onClick={() => setVisibleKeys((v) => ({ ...v, [app.id]: !v[app.id] }))}
                                                    className="p-1 text-[var(--color-text-tertiary)] hover:text-[var(--color-text-secondary)] transition-colors"
                                                >
                                                    {visibleKeys[app.id] ? <EyeOff size={13} /> : <Eye size={13} />}
                                                </button>
                                                <button
                                                    onClick={() => copyToClipboard(app.api_key, app.id)}
                                                    className="p-1 text-[var(--color-text-tertiary)] hover:text-[var(--color-text-secondary)] transition-colors"
                                                >
                                                    {copied === app.id ? <Check size={13} className="text-[var(--color-green)]" /> : <Copy size={13} />}
                                                </button>
                                            </div>
                                        </div>
                                    </div>

                                    {/* Stats Row */}
                                    <div className="flex items-center gap-6 mt-4 text-[11px] text-[var(--color-text-tertiary)]">
                                        <span className="flex items-center gap-1.5">
                                            <Zap size={11} />
                                            {app.rate_limit.toLocaleString()} req/min
                                        </span>
                                        <span>
                                            Created {new Date(app.created_at).toLocaleDateString()}
                                        </span>
                                    </div>
                                </div>
                            ))}
                        </div>
                    )}

                    {/* Quick Reference */}
                    <div className="mt-8">
                        <h3 className="text-sm font-medium mb-3">Quick Reference</h3>
                        <div className="code-block p-5">
                            <div className="flex items-center gap-2 mb-3">
                                <Terminal size={14} className="text-[var(--color-text-tertiary)]" />
                                <span className="text-xs font-mono text-[var(--color-text-tertiary)]">Using your API key</span>
                            </div>
                            <pre className="text-xs font-mono leading-relaxed text-[var(--color-text-secondary)] overflow-x-auto">
                                {`# REST API
curl -X POST https://plumbr.ca/api/redact \\
  -H "Authorization: Bearer YOUR_API_KEY" \\
  -H "Content-Type: application/json" \\
  -d '{"text": "password=secret123"}'

# Python
from plumbrc import Plumbr
p = Plumbr()
safe = p.redact("password=secret123")`}
                            </pre>
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
                        <Link href="/developers" className="hover:text-[var(--color-text)] transition-colors">Developers</Link>
                    </div>
                </div>
            </footer>
        </main>
    );
}
