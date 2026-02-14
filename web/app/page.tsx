"use client";

import { useState, useEffect, useRef } from "react";
import Link from "next/link";
import {
  Zap,
  HardDrive,
  Link2,
  Shield,
  Cpu,
  Package,
  Search,
  Timer,
  Brain,
  Cloud,
  MessageCircle,
  CreditCard,
  GitBranch,
  Server,
  Lock,
  Key,
  Smartphone,
  Database,
  BarChart3,
  User,
  KeyRound,
  ArrowRight,
  Terminal,
  Copy,
  ExternalLink,
  Globe,
  AlertTriangle,
  ChevronRight,
  Menu,
  X,
} from "lucide-react";

/* ── Animated Counter ── */
function Counter({ end, suffix = "", duration = 2000 }: { end: number; suffix?: string; duration?: number }) {
  const [count, setCount] = useState(0);
  const ref = useRef<HTMLSpanElement>(null);
  const started = useRef(false);

  useEffect(() => {
    const observer = new IntersectionObserver(
      ([entry]) => {
        if (entry.isIntersecting && !started.current) {
          started.current = true;
          const start = performance.now();
          const animate = (now: number) => {
            const progress = Math.min((now - start) / duration, 1);
            const eased = 1 - Math.pow(1 - progress, 3);
            setCount(Math.floor(eased * end));
            if (progress < 1) requestAnimationFrame(animate);
          };
          requestAnimationFrame(animate);
        }
      },
      { threshold: 0.5 }
    );
    if (ref.current) observer.observe(ref.current);
    return () => observer.disconnect();
  }, [end, duration]);

  return <span ref={ref} className="tabular-nums">{count.toLocaleString()}{suffix}</span>;
}

/* ── Typing Demo ── */
function TypingDemo() {
  const pairs = [
    { input: 'password=P@ssw0rd!', output: '[REDACTED:password]' },
    { input: 'key=AKIAIOSFODNN7EXAMPL3', output: 'key=[REDACTED:aws_key]' },
    { input: 'email=admin@company.com', output: 'email=[REDACTED:email]' },
    { input: 'token=ghp_aBcDeFgHiJkLm', output: 'token=[REDACTED:github]' },
  ];

  const [current, setCurrent] = useState(0);
  const [phase, setPhase] = useState<'typing' | 'redacting' | 'pausing'>('typing');
  const [chars, setChars] = useState(0);

  useEffect(() => {
    const pair = pairs[current];
    if (phase === 'typing') {
      if (chars < pair.input.length) {
        const timer = setTimeout(() => setChars(c => c + 1), 40);
        return () => clearTimeout(timer);
      } else {
        const timer = setTimeout(() => setPhase('redacting'), 400);
        return () => clearTimeout(timer);
      }
    }
    if (phase === 'redacting') {
      const timer = setTimeout(() => setPhase('pausing'), 600);
      return () => clearTimeout(timer);
    }
    if (phase === 'pausing') {
      const timer = setTimeout(() => {
        setCurrent((c) => (c + 1) % pairs.length);
        setPhase('typing');
        setChars(0);
      }, 1500);
      return () => clearTimeout(timer);
    }
  }, [phase, chars, current]);

  const pair = pairs[current];

  return (
    <div className="w-full max-w-md mx-auto">
      <div className="terminal">
        <div className="terminal-header">
          <div className="flex gap-1.5">
            <div className="w-2.5 h-2.5 rounded-full bg-[#555]" />
            <div className="w-2.5 h-2.5 rounded-full bg-[#555]" />
            <div className="w-2.5 h-2.5 rounded-full bg-[#555]" />
          </div>
          <Terminal size={12} className="text-[var(--color-text-tertiary)] ml-1" />
          <span className="text-[11px] text-[var(--color-text-tertiary)] font-mono">plumbr</span>
        </div>
        <div className="terminal-body min-h-[72px] flex flex-col justify-center gap-1.5">
          <div className="flex items-center gap-3">
            <span className="text-[var(--color-text-tertiary)] text-xs w-6">in</span>
            <span className="text-[var(--color-text)] text-sm">
              {pair.input.slice(0, chars)}
              {phase === 'typing' && <span className="inline-block w-[2px] h-[14px] bg-[var(--color-text-secondary)] animate-blink ml-[1px] align-middle" />}
            </span>
          </div>
          {(phase === 'redacting' || phase === 'pausing') && (
            <div className="flex items-center gap-3 animate-fade-up">
              <span className="text-[var(--color-text-tertiary)] text-xs w-6">out</span>
              <span className="text-sm">
                {pair.output.split(/(\[REDACTED:[^\]]+\])/).map((part, i) =>
                  part.startsWith('[REDACTED') ? (
                    <span key={i} className="redacted-tag">{part}</span>
                  ) : (
                    <span key={i} className="text-[var(--color-text)]">{part}</span>
                  )
                )}
              </span>
            </div>
          )}
        </div>
      </div>
    </div>
  );
}

/* ── Feature Card ── */
function FeatureCard({ title, desc, icon }: { title: string; desc: string; icon: React.ReactNode }) {
  return (
    <div className="card p-5">
      <div className="w-9 h-9 rounded-lg bg-[var(--color-bg-elevated)] flex items-center justify-center mb-4 text-[var(--color-text-secondary)]">
        {icon}
      </div>
      <h3 className="text-[15px] font-medium text-[var(--color-text)] mb-1.5">{title}</h3>
      <p className="text-[13px] text-[var(--color-text-secondary)] leading-relaxed">{desc}</p>
    </div>
  );
}

/* ── Pattern Categories ── */
const categories = [
  { name: "Cloud", count: 109, icon: Cloud },
  { name: "Communication", count: 127, icon: MessageCircle },
  { name: "Payment", count: 79, icon: CreditCard },
  { name: "VCS", count: 63, icon: GitBranch },
  { name: "Infrastructure", count: 72, icon: Server },
  { name: "Crypto", count: 54, icon: Lock },
  { name: "Secrets", count: 52, icon: Key },
  { name: "Social", count: 50, icon: Smartphone },
  { name: "Database", count: 42, icon: Database },
  { name: "Analytics", count: 30, icon: BarChart3 },
  { name: "PII", count: 14, icon: User },
  { name: "Auth", count: 10, icon: KeyRound },
];

export default function Home() {
  const [mobileMenuOpen, setMobileMenuOpen] = useState(false);

  return (
    <main className="min-h-screen">
      {/* ── Nav ── */}
      <nav className="fixed top-0 left-0 right-0 z-50 nav-bar">
        <div className="max-w-5xl mx-auto px-4 sm:px-6 h-14 flex items-center justify-between">
          <Link href="/" className="flex items-center gap-2.5">
            <div className="w-7 h-7 rounded-md flex items-center justify-center" style={{ background: '#e8e8e8' }}>
              <span className="text-xs font-bold" style={{ color: '#141414' }}>P</span>
            </div>
            <span className="font-semibold text-[15px]">PlumbrC</span>
          </Link>
          <div className="flex items-center gap-1">
            <a href="#features" className="text-[13px] text-[var(--color-text-secondary)] hover:text-[var(--color-text)] transition-colors px-3 py-1.5 rounded-md hover:bg-[var(--color-bg-elevated)] hidden md:block">Features</a>
            <a href="#patterns" className="text-[13px] text-[var(--color-text-secondary)] hover:text-[var(--color-text)] transition-colors px-3 py-1.5 rounded-md hover:bg-[var(--color-bg-elevated)] hidden md:block">Patterns</a>
            <a href="#python" className="text-[13px] text-[var(--color-text-secondary)] hover:text-[var(--color-text)] transition-colors px-3 py-1.5 rounded-md hover:bg-[var(--color-bg-elevated)] hidden md:block">Python</a>
            <a href="#api" className="text-[13px] text-[var(--color-text-secondary)] hover:text-[var(--color-text)] transition-colors px-3 py-1.5 rounded-md hover:bg-[var(--color-bg-elevated)] hidden md:block">API</a>
            <Link href="/docs" className="text-[13px] text-[var(--color-text-secondary)] hover:text-[var(--color-text)] transition-colors px-3 py-1.5 rounded-md hover:bg-[var(--color-bg-elevated)] hidden md:block">Docs</Link>
            <Link href="/developers" className="text-[13px] text-[var(--color-text-secondary)] hover:text-[var(--color-text)] transition-colors px-3 py-1.5 rounded-md hover:bg-[var(--color-bg-elevated)] hidden md:block">Developers</Link>
            <Link
              href="/playground"
              className="text-[13px] font-medium px-4 py-1.5 rounded-md transition-opacity hover:opacity-90 ml-2 hidden sm:inline-flex"
              style={{ background: '#e8e8e8', color: '#141414' }}
            >
              Playground
            </Link>
            <button
              onClick={() => setMobileMenuOpen(!mobileMenuOpen)}
              className="md:hidden p-2 text-[var(--color-text-secondary)] hover:text-[var(--color-text)] transition-colors ml-1"
              aria-label="Toggle menu"
            >
              {mobileMenuOpen ? <X size={20} /> : <Menu size={20} />}
            </button>
          </div>
        </div>
        {/* Mobile menu */}
        {mobileMenuOpen && (
          <div className="md:hidden border-t border-[var(--color-border)] bg-[var(--color-bg)]/95 backdrop-blur-xl">
            <div className="px-4 py-3 space-y-1">
              {[
                { href: "#features", label: "Features" },
                { href: "#patterns", label: "Patterns" },
                { href: "#python", label: "Python" },
                { href: "#api", label: "API" },
              ].map((item) => (
                <a
                  key={item.label}
                  href={item.href}
                  onClick={() => setMobileMenuOpen(false)}
                  className="block text-[14px] text-[var(--color-text-secondary)] hover:text-[var(--color-text)] py-2 px-3 rounded-lg hover:bg-[var(--color-bg-elevated)] transition-colors"
                >
                  {item.label}
                </a>
              ))}
              <div className="border-t border-[var(--color-border)] pt-2 mt-2 space-y-1">
                <Link href="/docs" onClick={() => setMobileMenuOpen(false)} className="block text-[14px] text-[var(--color-text-secondary)] hover:text-[var(--color-text)] py-2 px-3 rounded-lg hover:bg-[var(--color-bg-elevated)] transition-colors">Docs</Link>
                <Link href="/developers" onClick={() => setMobileMenuOpen(false)} className="block text-[14px] text-[var(--color-text-secondary)] hover:text-[var(--color-text)] py-2 px-3 rounded-lg hover:bg-[var(--color-bg-elevated)] transition-colors">Developers</Link>
                <Link href="/playground" onClick={() => setMobileMenuOpen(false)} className="block text-[14px] font-medium py-2 px-3 rounded-lg transition-colors" style={{ color: '#e8e8e8' }}>Playground →</Link>
              </div>
            </div>
          </div>
        )}
      </nav>

      {/* ── Hero ── */}
      <section className="pt-28 sm:pt-32 pb-14 sm:pb-20 px-4 sm:px-6">
        <div className="max-w-2xl mx-auto text-center">
          <div className="inline-flex items-center gap-2 text-[13px] text-[var(--color-text-secondary)] border border-[var(--color-border)] rounded-full px-4 py-1.5 mb-8">
            <div className="w-1.5 h-1.5 rounded-full bg-[var(--color-green)] animate-pulse-dot" />
            Open Source · 702 Patterns · Pure C · AMD Optimized
          </div>

          <h1 className="text-4xl sm:text-5xl lg:text-[56px] font-bold tracking-tight leading-[1.1] mb-5">
            Stop leaking secrets
            <br />
            in your logs
          </h1>

          <p className="text-base text-[var(--color-text-secondary)] max-w-md mx-auto mb-10 leading-relaxed">
            Scan log streams for AWS keys, passwords, tokens, and 700+ other patterns. Up to 5M lines/sec with AVX2 SIMD on AMD Ryzen.
          </p>

          <div className="flex flex-col sm:flex-row items-center justify-center gap-3 mb-16">
            <Link
              href="/playground"
              className="font-medium px-6 py-2.5 rounded-lg transition-opacity hover:opacity-90 text-[15px] flex items-center gap-2"
              style={{ background: '#e8e8e8', color: '#141414' }}
            >
              Try the Playground
              <ArrowRight size={16} />
            </Link>
            <a
              href="#api"
              className="border border-[var(--color-border)] text-[var(--color-text)] font-medium px-6 py-2.5 rounded-lg hover:bg-[var(--color-bg-raised)] transition-colors text-[15px]"
            >
              View API
            </a>
          </div>

          <TypingDemo />
        </div>
      </section>

      {/* ── Stats ── */}
      <section className="divider">
        <div className="max-w-5xl mx-auto grid grid-cols-3 md:grid-cols-5">
          {[
            { icon: <Zap size={20} />, value: <>5M+</>, label: "lines/sec (14 patterns)" },
            { icon: <Cpu size={20} />, value: <><Counter end={2} />.6M</>, label: "lines/sec (702 patterns)" },
            { icon: <Search size={20} />, value: <Counter end={702} />, label: "detection patterns" },
            { icon: <Timer size={20} />, value: <>364</>, label: "MB/s throughput" },
            { icon: <Brain size={20} />, value: "0", label: "heap allocs" },
          ].map((stat, i) => (
            <div key={i} className="text-center py-8 px-4">
              <div className="text-[var(--color-text-tertiary)] flex justify-center mb-3">{stat.icon}</div>
              <div className="text-2xl sm:text-3xl font-semibold text-[var(--color-text)] mb-1">{stat.value}</div>
              <div className="text-xs text-[var(--color-text-tertiary)]">{stat.label}</div>
            </div>
          ))}
        </div>
      </section>

      {/* ── Pipeline ── */}
      <section className="py-20 px-6 divider">
        <div className="max-w-4xl mx-auto text-center">
          <h2 className="text-2xl font-semibold mb-3">Three-phase detection pipeline</h2>
          <p className="text-sm text-[var(--color-text-secondary)] mb-10 max-w-lg mx-auto">
            SSE 4.2 hardware pre-filter rejects clean lines instantly. Aho-Corasick scans survivors in a single pass. PCRE2 JIT only fires on the ~5% with a hit.
          </p>
          <div className="flex items-center justify-start sm:justify-center gap-2 font-mono text-xs sm:text-sm overflow-x-auto pb-2 px-2">
            {["stdin", "SSE 4.2 Filter", "Aho-Corasick", "PCRE2 JIT", "Redact", "stdout"].map((label, i, arr) => (
              <div key={i} className="flex items-center gap-2">
                <span className={`px-3 py-1.5 rounded-md border text-[var(--color-text-secondary)] ${label === 'SSE 4.2 Filter' ? 'border-[var(--color-text-tertiary)]' : 'border-[var(--color-border)]'
                  }`}>{label}</span>
                {i < arr.length - 1 && <ArrowRight size={14} className="text-[var(--color-text-tertiary)]" />}
              </div>
            ))}
          </div>
          <p className="text-[11px] text-[var(--color-text-tertiary)] mt-4">In real-world logs (~3% secrets), the SSE 4.2 filter skips 97% of lines before they reach Aho-Corasick</p>
        </div>
      </section>

      {/* ── Features ── */}
      <section id="features" className="py-20 px-6">
        <div className="max-w-5xl mx-auto">
          <div className="text-center mb-12">
            <h2 className="text-2xl font-semibold mb-3">Built for production</h2>
            <p className="text-sm text-[var(--color-text-secondary)]">No compromises on speed, safety, or simplicity.</p>
          </div>

          <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-3">
            <FeatureCard icon={<Zap size={18} />} title="O(n) Multi-Pattern" desc="Aho-Corasick DFA scans for all 702 patterns in a single pass. Linear time regardless of pattern count." />
            <FeatureCard icon={<Cpu size={18} />} title="AVX2 + SSE 4.2 SIMD" desc="Hardware-accelerated byte scanning and line pre-filtering. Optimized for AMD Ryzen, works on any x86-64." />
            <FeatureCard icon={<HardDrive size={18} />} title="Zero Allocations" desc="Arena allocator via mmap. No malloc, no free, no GC in the processing hot path." />
            <FeatureCard icon={<Shield size={18} />} title="ReDoS Protected" desc="Match limits on every regex. No catastrophic backtracking, even with adversarial input." />
            <FeatureCard icon={<Link2 size={18} />} title="Pipeline Native" desc="stdin to stdout. Works with tail -f, docker logs, kubectl, Fluentd, Vector, Logstash." />
            <FeatureCard icon={<Package size={18} />} title="3 Ways to Use" desc="Native C binary for max speed. Python package via pip. REST API for testing and prototyping." />
          </div>
        </div>
      </section>

      {/* ── Patterns ── */}
      <section id="patterns" className="py-20 px-6 divider">
        <div className="max-w-4xl mx-auto">
          <div className="text-center mb-12">
            <h2 className="text-2xl font-semibold mb-3">702 detection patterns</h2>
            <p className="text-sm text-[var(--color-text-secondary)]">12 categories. Load all, or pick what you need.</p>
          </div>

          <div className="grid grid-cols-2 sm:grid-cols-3 lg:grid-cols-4 gap-2.5">
            {categories.map((cat) => {
              const Icon = cat.icon;
              return (
                <div key={cat.name} className="card px-4 py-3 flex items-center justify-between hover:border-[var(--color-border-light)]">
                  <span className="text-[13px] flex items-center gap-2.5 text-[var(--color-text)]">
                    <Icon size={14} className="text-[var(--color-text-secondary)]" />
                    {cat.name}
                  </span>
                  <span className="text-[11px] font-mono text-[var(--color-text-tertiary)]">{cat.count}</span>
                </div>
              );
            })}
          </div>
        </div>
      </section>

      {/* ── Benchmarks ── */}
      <section id="benchmarks" className="py-20 px-6">
        <div className="max-w-5xl mx-auto">
          <div className="text-center mb-12">
            <h2 className="text-2xl font-semibold mb-3">Real benchmarks, real numbers</h2>
            <p className="text-sm text-[var(--color-text-secondary)] max-w-lg mx-auto">Tested on AMD Ryzen 5 5600X (6C/12T). Real-world workload: 1M log lines, 3% contain secrets.</p>
          </div>

          <div className="grid grid-cols-1 md:grid-cols-2 gap-4 mb-8">
            {/* 14 patterns */}
            <div className="card p-6">
              <div className="flex items-center justify-between mb-4">
                <h3 className="text-[15px] font-semibold">Default (14 patterns)</h3>
                <span className="text-[10px] font-bold px-2 py-0.5 rounded-full" style={{ background: '#e8e8e8', color: '#141414' }}>FASTEST</span>
              </div>
              <p className="text-[12px] text-[var(--color-text-tertiary)] mb-4">AWS, GitHub, email, IP, JWT, passwords — covers 90% of use cases</p>
              <div className="space-y-3">
                <div className="flex items-center justify-between">
                  <span className="text-[12px] text-[var(--color-text-secondary)]">Single-threaded</span>
                  <span className="text-[14px] font-semibold" style={{ color: '#e8e8e8' }}>3.7M lines/sec</span>
                </div>
                <div className="w-full bg-[var(--color-bg-elevated)] rounded-full h-1.5"><div className="h-1.5 rounded-full" style={{ width: '74%', background: '#e8e8e8' }} /></div>
                <div className="flex items-center justify-between">
                  <span className="text-[12px] text-[var(--color-text-secondary)]">Multi-threaded (8T)</span>
                  <span className="text-[14px] font-semibold" style={{ color: '#e8e8e8' }}>4.9M lines/sec</span>
                </div>
                <div className="w-full bg-[var(--color-bg-elevated)] rounded-full h-1.5"><div className="h-1.5 rounded-full" style={{ width: '98%', background: '#e8e8e8' }} /></div>
              </div>
            </div>

            {/* 702 patterns */}
            <div className="card p-6">
              <div className="flex items-center justify-between mb-4">
                <h3 className="text-[15px] font-semibold">Full scan (702 patterns)</h3>
                <span className="text-[10px] font-mono px-2 py-0.5 rounded-full border border-[var(--color-border)] text-[var(--color-text-tertiary)]">ALL PATTERNS</span>
              </div>
              <p className="text-[12px] text-[var(--color-text-tertiary)] mb-4">Every cloud, payment, VCS, infra, PII, and auth pattern loaded</p>
              <div className="space-y-3">
                <div className="flex items-center justify-between">
                  <span className="text-[12px] text-[var(--color-text-secondary)]">Single-threaded</span>
                  <span className="text-[14px] font-semibold" style={{ color: '#e8e8e8' }}>1.1M lines/sec</span>
                </div>
                <div className="w-full bg-[var(--color-bg-elevated)] rounded-full h-1.5"><div className="h-1.5 rounded-full" style={{ width: '22%', background: '#e8e8e8' }} /></div>
                <div className="flex items-center justify-between">
                  <span className="text-[12px] text-[var(--color-text-secondary)]">Multi-threaded (8T)</span>
                  <span className="text-[14px] font-semibold" style={{ color: '#e8e8e8' }}>2.6M lines/sec</span>
                </div>
                <div className="w-full bg-[var(--color-bg-elevated)] rounded-full h-1.5"><div className="h-1.5 rounded-full" style={{ width: '52%', background: '#e8e8e8' }} /></div>
              </div>
            </div>
          </div>

          {/* Integration comparison */}
          <div className="grid grid-cols-1 md:grid-cols-3 gap-3">
            <div className="card p-5 text-center">
              <Cpu size={20} className="mx-auto text-[var(--color-text-secondary)] mb-3" />
              <div className="text-lg font-bold mb-0.5" style={{ color: '#e8e8e8' }}>5M lines/sec</div>
              <div className="text-[11px] text-[var(--color-text-tertiary)] mb-2">Native C Binary</div>
              <p className="text-[11px] text-[var(--color-text-secondary)]">AVX2 SIMD, SSE 4.2, AMD Ryzen optimized. For maximum throughput.</p>
            </div>
            <div className="card p-5 text-center">
              <Package size={20} className="mx-auto text-[var(--color-text-secondary)] mb-3" />
              <div className="text-lg font-bold mb-0.5" style={{ color: '#e8e8e8' }}>83K lines/sec</div>
              <div className="text-[11px] text-[var(--color-text-tertiary)] mb-2">Python Package</div>
              <p className="text-[11px] text-[var(--color-text-secondary)]">Same C engine via ctypes. No network overhead. pip install plumbrc.</p>
            </div>
            <div className="card p-5 text-center">
              <Globe size={20} className="mx-auto text-[var(--color-text-secondary)] mb-3" />
              <div className="text-lg font-bold mb-0.5" style={{ color: '#e8e8e8' }}>~0.1ms</div>
              <div className="text-[11px] text-[var(--color-text-tertiary)] mb-2">REST API (server-side)</div>
              <p className="text-[11px] text-[var(--color-text-secondary)]">Network latency is the bottleneck. Best for testing and prototyping.</p>
            </div>
          </div>
        </div>
      </section>

      {/* ── API ── */}
      <section id="api" className="py-20 px-6 divider">
        <div className="max-w-3xl mx-auto">
          <div className="text-center mb-12">
            <h2 className="text-2xl font-semibold mb-3">REST API</h2>
            <p className="text-sm text-[var(--color-text-secondary)]">Send text, get clean text back. Sub-millisecond processing on the server.</p>
          </div>

          <div className="code-block p-5">
            <div className="flex items-center justify-between mb-4">
              <div className="font-mono text-sm">
                <span className="text-[var(--color-green)] font-medium">POST</span>{" "}
                <span className="text-[var(--color-text-secondary)]">/api/redact</span>
              </div>
              <button className="text-[var(--color-text-tertiary)] hover:text-[var(--color-text-secondary)] transition-colors">
                <Copy size={14} />
              </button>
            </div>
            <pre className="text-xs font-mono leading-relaxed text-[var(--color-text-secondary)] overflow-x-auto">
              {`curl -X POST https://plumbr.ca/api/redact \\
  -H "Content-Type: application/json" \\
  -d '{"text": "key=AKIAIOSFODNN7EXAMPL3"}'

# Response:
{
  "redacted": "key=[REDACTED:aws_access_key]",
  "stats": {
    "lines_modified": 1,
    "patterns_matched": 1,
    "processing_time_ms": 0.066
  }
}`}
            </pre>
          </div>

          <div className="card p-4 mt-4 flex items-start gap-3">
            <AlertTriangle size={16} className="text-[var(--color-text-secondary)] shrink-0 mt-0.5" />
            <p className="text-[12px] text-[var(--color-text-secondary)] leading-relaxed">
              <strong>Honest note:</strong> The server processes each request in ~0.1ms, but network round-trip adds 100-800ms depending on your location.
              For production throughput, use the <strong>native C binary</strong> or <strong>Python package</strong> — both run locally with zero network overhead.
            </p>
          </div>

          <div className="text-center mt-8">
            <Link
              href="/developers"
              className="text-[13px] font-medium px-5 py-2 rounded-lg transition-opacity hover:opacity-90 inline-flex items-center gap-2"
              style={{ background: '#e8e8e8', color: '#141414' }}
            >
              Get your API key
              <ArrowRight size={14} />
            </Link>
          </div>
        </div>
      </section>

      {/* ── Python Package ── */}
      <section id="python" className="py-20 px-6 divider">
        <div className="max-w-3xl mx-auto">
          <div className="text-center mb-12">
            <h2 className="text-2xl font-semibold mb-3">Python Package</h2>
            <p className="text-sm text-[var(--color-text-secondary)]">pip install and start redacting in seconds.</p>
          </div>

          {/* Installation */}
          <div className="code-block p-5 mb-6">
            <div className="flex items-center justify-between mb-4">
              <div className="font-mono text-sm text-[var(--color-text)]">
                Installation
              </div>
              <button className="text-[var(--color-text-tertiary)] hover:text-[var(--color-text-secondary)] transition-colors">
                <Copy size={14} />
              </button>
            </div>
            <pre className="text-xs font-mono leading-relaxed text-[var(--color-text-secondary)] overflow-x-auto">
              {`pip install plumbrc`}
            </pre>
          </div>

          {/* Usage Example */}
          <div className="code-block p-5 mb-6">
            <div className="flex items-center justify-between mb-4">
              <div className="font-mono text-sm text-[var(--color-text)]">
                Usage
              </div>
              <button className="text-[var(--color-text-tertiary)] hover:text-[var(--color-text-secondary)] transition-colors">
                <Copy size={14} />
              </button>
            </div>
            <pre className="text-xs font-mono leading-relaxed text-[var(--color-text-secondary)] overflow-x-auto">
              {`from plumbrc import Plumbr

# Initialize
p = Plumbr()

# Redact secrets
safe = p.redact("password=secret123")
print(safe)  # "[REDACTED:password]"

# Batch processing
lines = ["api_key=secret", "normal log", "token=xyz"]
safe_lines = p.redact_lines(lines)

# Context manager
with Plumbr() as p:
    safe = p.redact("AWS_KEY=AKIAIOSFODNN7EXAMPLE")

# Get statistics
print(p.pattern_count)  # 14
print(p.stats)          # {'lines_processed': 1, ...}`}
            </pre>
          </div>

          {/* Performance Stats */}
          <div className="grid grid-cols-1 sm:grid-cols-3 gap-2.5">
            {[
              { icon: <Zap size={16} />, title: "83K lines/sec", desc: "C engine under the hood" },
              { icon: <Timer size={16} />, title: "12 µs", desc: "Per line" },
              { icon: <Package size={16} />, title: "Zero deps", desc: "Pure ctypes FFI" },
            ].map((item) => (
              <div key={item.title} className="card p-4 text-center hover:border-[var(--color-border-light)]">
                <div className="flex justify-center text-[var(--color-text-secondary)] mb-2">{item.icon}</div>
                <div className="text-sm font-medium">{item.title}</div>
                <div className="text-[11px] text-[var(--color-text-tertiary)]">{item.desc}</div>
              </div>
            ))}
          </div>
        </div>
      </section>


      {/* ── CTA ── */}
      <section className="py-20 px-6 divider">
        <div className="max-w-md mx-auto text-center">
          <h2 className="text-2xl font-semibold mb-4">Ready to ship clean logs?</h2>
          <p className="text-sm text-[var(--color-text-secondary)] mb-8">Try in the browser, install via pip, or integrate the API.</p>
          <div className="flex flex-col sm:flex-row items-center justify-center gap-3">
            <Link
              href="/playground"
              className="font-medium px-6 py-2.5 rounded-lg transition-opacity hover:opacity-90 text-[15px] flex items-center gap-2"
              style={{ background: '#e8e8e8', color: '#141414' }}
            >
              Open Playground
              <ArrowRight size={16} />
            </Link>
            <div className="code-block px-5 py-2.5 text-sm font-mono flex items-center gap-2">
              <Terminal size={14} className="text-[var(--color-text-tertiary)]" />
              <span className="text-[var(--color-text-secondary)]">pip install plumbrc</span>
            </div>
          </div>
        </div>
      </section>

      {/* ── Footer ── */}
      <footer className="divider py-6 px-6">
        <div className="max-w-5xl mx-auto flex flex-col sm:flex-row items-center justify-between gap-4">
          <div className="flex items-center gap-2.5">
            <div className="w-5 h-5 rounded flex items-center justify-center" style={{ background: '#e8e8e8' }}>
              <span className="text-[9px] font-bold" style={{ color: '#141414' }}>P</span>
            </div>
            <span className="text-sm font-medium">PlumbrC</span>
            <span className="text-[11px] text-[var(--color-text-tertiary)]">Protect your logs.</span>
          </div>
          <div className="flex items-center gap-5 text-[13px] text-[var(--color-text-secondary)]">
            <a href="https://github.com/AmritRai1234/plumbrC" target="_blank" rel="noopener" className="hover:text-[var(--color-text)] transition-colors flex items-center gap-1.5">
              GitHub <ExternalLink size={11} />
            </a>
            <Link href="/playground" className="hover:text-[var(--color-text)] transition-colors">Playground</Link>
            <a href="#python" className="hover:text-[var(--color-text)] transition-colors">Python</a>
            <a href="#api" className="hover:text-[var(--color-text)] transition-colors">API</a>
          </div>
        </div>
      </footer>
    </main>
  );
}
