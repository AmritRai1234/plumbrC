"use client";

import { useState, useEffect } from "react";
import Link from "next/link";
import {
    Terminal,
    Code2,
    Package,
    Globe,
    Server,
    BookOpen,
    Copy,
    Check,
    ChevronRight,
    Cpu,
    FileText,
    Layers,
    ArrowLeft,
    Zap,
    Shield,
} from "lucide-react";

/* ─── Copy button ───────────────────────────────────────────── */
function CopyBtn({ text }: { text: string }) {
    const [copied, setCopied] = useState(false);
    return (
        <button
            onClick={() => {
                navigator.clipboard.writeText(text);
                setCopied(true);
                setTimeout(() => setCopied(false), 2000);
            }}
            className="absolute top-3 right-3 p-1.5 rounded bg-white/5 hover:bg-white/10 transition-colors text-zinc-400 hover:text-white"
            title="Copy"
        >
            {copied ? <Check size={14} /> : <Copy size={14} />}
        </button>
    );
}

/* ─── Code block ────────────────────────────────────────────── */
function Code({
    children,
    lang = "bash",
}: {
    children: string;
    lang?: string;
}) {
    return (
        <div className="relative group rounded-lg bg-black/40 border border-white/5 overflow-hidden my-4">
            <div className="flex items-center gap-2 px-4 py-2 bg-white/[0.02] border-b border-white/5 text-xs text-zinc-500">
                <span>{lang}</span>
            </div>
            <CopyBtn text={children.trim()} />
            <pre className="p-3 sm:p-4 overflow-x-auto text-xs sm:text-sm leading-relaxed">
                <code className="text-zinc-300">{children.trim()}</code>
            </pre>
        </div>
    );
}

/* ─── Section nav items ─────────────────────────────────────── */
const sections = [
    { id: "quickstart", label: "Quick Start", icon: Zap },
    { id: "cli", label: "CLI Usage", icon: Terminal },
    { id: "c-library", label: "C Library", icon: Code2 },
    { id: "python", label: "Python Package", icon: Package },
    { id: "rest-api", label: "REST API", icon: Globe },
    { id: "patterns", label: "Custom Patterns", icon: FileText },
    { id: "integrations", label: "Integrations", icon: Layers },
    { id: "building", label: "Building from Source", icon: Server },
    { id: "performance", label: "Performance", icon: Cpu },
];

export default function DocsPage() {
    const [activeSection, setActiveSection] = useState("quickstart");

    useEffect(() => {
        const observer = new IntersectionObserver(
            (entries) => {
                for (const entry of entries) {
                    if (entry.isIntersecting) {
                        setActiveSection(entry.target.id);
                    }
                }
            },
            { rootMargin: "-20% 0px -60% 0px" }
        );
        sections.forEach((s) => {
            const el = document.getElementById(s.id);
            if (el) observer.observe(el);
        });
        return () => observer.disconnect();
    }, []);

    const [mobileNavOpen, setMobileNavOpen] = useState(false);

    return (
        <div className="min-h-screen bg-[#0a0a0f] text-zinc-200">
            {/* Header */}
            <header className="sticky top-0 z-50 bg-[#0a0a0f]/80 backdrop-blur-xl border-b border-white/5">
                <div className="max-w-7xl mx-auto px-4 sm:px-6 h-14 flex items-center justify-between">
                    <div className="flex items-center gap-4">
                        <Link
                            href="/"
                            className="flex items-center gap-2 text-zinc-400 hover:text-white transition-colors text-sm"
                        >
                            <ArrowLeft size={16} />
                            Back
                        </Link>
                        <div className="w-px h-5 bg-white/10" />
                        <div className="flex items-center gap-2">
                            <BookOpen size={16} className="text-blue-400" />
                            <span className="font-semibold text-white">Documentation</span>
                        </div>
                    </div>
                    <Link
                        href="/playground"
                        className="text-sm text-zinc-400 hover:text-white transition-colors"
                    >
                        Playground →
                    </Link>
                </div>
            </header>

            {/* Mobile section nav */}
            <div className="lg:hidden sticky top-14 z-40 bg-[#0a0a0f]/90 backdrop-blur-xl border-b border-white/5">
                <div className="max-w-7xl mx-auto px-4 sm:px-6">
                    <button
                        onClick={() => setMobileNavOpen(!mobileNavOpen)}
                        className="w-full flex items-center justify-between py-3 text-sm"
                    >
                        <span className="flex items-center gap-2 text-blue-400 font-medium">
                            {(() => {
                                const s = sections.find((s) => s.id === activeSection);
                                if (s) {
                                    const Icon = s.icon;
                                    return <><Icon size={14} />{s.label}</>;
                                }
                                return "Sections";
                            })()}
                        </span>
                        <ChevronRight size={14} className={`text-zinc-500 transition-transform ${mobileNavOpen ? 'rotate-90' : ''}`} />
                    </button>
                    {mobileNavOpen && (
                        <div className="pb-3 space-y-0.5">
                            {sections.map((s) => {
                                const Icon = s.icon;
                                const active = activeSection === s.id;
                                return (
                                    <a
                                        key={s.id}
                                        href={`#${s.id}`}
                                        onClick={() => setMobileNavOpen(false)}
                                        className={`flex items-center gap-2.5 px-3 py-2 rounded-lg text-sm transition-all ${active
                                            ? "bg-blue-500/10 text-blue-400 font-medium"
                                            : "text-zinc-500 hover:text-zinc-300 hover:bg-white/[0.03]"
                                            }`}
                                    >
                                        <Icon size={15} />
                                        {s.label}
                                    </a>
                                );
                            })}
                        </div>
                    )}
                </div>
            </div>

            <div className="max-w-7xl mx-auto px-4 sm:px-6 py-8 sm:py-12 flex gap-12">
                {/* Sidebar nav */}
                <nav className="hidden lg:block w-56 shrink-0">
                    <div className="sticky top-24 space-y-1">
                        {sections.map((s) => {
                            const Icon = s.icon;
                            const active = activeSection === s.id;
                            return (
                                <a
                                    key={s.id}
                                    href={`#${s.id}`}
                                    className={`flex items-center gap-2.5 px-3 py-2 rounded-lg text-sm transition-all ${active
                                        ? "bg-blue-500/10 text-blue-400 font-medium"
                                        : "text-zinc-500 hover:text-zinc-300 hover:bg-white/[0.03]"
                                        }`}
                                >
                                    <Icon size={15} />
                                    {s.label}
                                </a>
                            );
                        })}
                    </div>
                </nav>

                {/* Main content */}
                <main className="flex-1 min-w-0 max-w-3xl">
                    {/* ─── QUICK START ─────────────────────────────── */}
                    <section id="quickstart" className="mb-20">
                        <h1 className="text-2xl sm:text-3xl font-bold text-white mb-2">
                            PlumbrC Documentation
                        </h1>
                        <p className="text-zinc-400 mb-8 text-lg">
                            Everything you need to detect and redact secrets from logs, streams,
                            and text data.
                        </p>

                        <div className="grid sm:grid-cols-3 gap-4 mb-10">
                            {[
                                {
                                    icon: Terminal,
                                    title: "CLI",
                                    desc: "Pipe or file mode",
                                    speed: "5M lines/sec",
                                },
                                {
                                    icon: Package,
                                    title: "Python",
                                    desc: "pip install plumbrc",
                                    speed: "83K lines/sec",
                                },
                                {
                                    icon: Globe,
                                    title: "REST API",
                                    desc: "POST /api/redact",
                                    speed: "~0.1ms server",
                                },
                            ].map((c) => (
                                <div
                                    key={c.title}
                                    className="p-4 rounded-xl bg-white/[0.02] border border-white/5 hover:border-white/10 transition-colors"
                                >
                                    <c.icon size={18} className="text-blue-400 mb-2" />
                                    <div className="font-semibold text-white text-sm">
                                        {c.title}
                                    </div>
                                    <div className="text-xs text-zinc-500">{c.desc}</div>
                                    <div className="text-xs text-emerald-400 mt-1 font-mono">
                                        {c.speed}
                                    </div>
                                </div>
                            ))}
                        </div>

                        <h2 className="text-xl font-semibold text-white mb-4">
                            Quick Start
                        </h2>

                        <h3 className="text-sm font-medium text-zinc-400 mb-2 flex items-center gap-2">
                            <Terminal size={14} /> Option 1: Native C Binary (fastest)
                        </h3>
                        <Code lang="bash">{`# Install dependencies
sudo apt install build-essential libpcre2-dev

# Clone and build
git clone https://github.com/AmritRai1234/plumbrC.git
cd plumbrC
make -j$(nproc)

# Run
echo "api_key=AKIAIOSFODNN7EXAMPLE" | ./build/bin/plumbr
# Output: api_key=[REDACTED:aws_key]`}</Code>

                        <h3 className="text-sm font-medium text-zinc-400 mb-2 mt-6 flex items-center gap-2">
                            <Package size={14} /> Option 2: Python Package
                        </h3>
                        <Code lang="bash">{`pip install plumbrc`}</Code>
                        <Code lang="python">{`from plumbrc import Plumbr

p = Plumbr()
result = p.redact("api_key=AKIAIOSFODNN7EXAMPLE")
print(result)  # api_key=[REDACTED:aws_key]`}</Code>

                        <h3 className="text-sm font-medium text-zinc-400 mb-2 mt-6 flex items-center gap-2">
                            <Globe size={14} /> Option 3: REST API (for testing)
                        </h3>
                        <Code lang="bash">{`curl -X POST https://plumbr.ca/api/redact \\
  -H "Content-Type: application/json" \\
  -d '{"text": "api_key=AKIAIOSFODNN7EXAMPLE"}'`}</Code>

                        <div className="mt-4 p-3 rounded-lg bg-amber-500/5 border border-amber-500/10 text-sm text-amber-400/80">
                            <strong className="text-amber-400">Note:</strong> The REST API is
                            for testing and prototyping. Network latency (100–800ms round-trip)
                            is the bottleneck — the server processes each request in ~0.1ms.
                            For production throughput, use the native C binary or Python
                            package.
                        </div>
                    </section>

                    {/* ─── CLI ─────────────────────────────────────── */}
                    <section id="cli" className="mb-20">
                        <h2 className="text-2xl font-bold text-white mb-2">CLI Usage</h2>
                        <p className="text-zinc-400 mb-6">
                            The <code className="text-blue-400 text-sm">plumbr</code> binary
                            reads from stdin and writes clean output to stdout. Perfect for
                            Unix pipelines.
                        </p>

                        <h3 className="text-lg font-semibold text-white mb-3">
                            Basic usage
                        </h3>
                        <Code lang="bash">{`# Pipe mode — redact in real-time
tail -f /var/log/app.log | plumbr | tee safe.log

# File mode — redact an entire file
plumbr < app.log > redacted.log

# Inline test
echo "password=s3cret123 token=ghp_abc123xyz" | plumbr`}</Code>

                        <h3 className="text-lg font-semibold text-white mb-3 mt-8">
                            Options
                        </h3>
                        <div className="overflow-x-auto">
                            <table className="w-full text-sm border border-white/5 rounded-lg overflow-hidden">
                                <thead>
                                    <tr className="bg-white/[0.03] text-zinc-400 text-left">
                                        <th className="px-4 py-2.5 font-medium">Flag</th>
                                        <th className="px-4 py-2.5 font-medium">Description</th>
                                        <th className="px-4 py-2.5 font-medium">Default</th>
                                    </tr>
                                </thead>
                                <tbody className="text-zinc-300">
                                    {[
                                        ["-p, --patterns FILE", "Load patterns from FILE", "built-in defaults"],
                                        ["-d, --defaults", "Use built-in default patterns (14)", "on"],
                                        ["-D, --no-defaults", "Disable built-in defaults", "—"],
                                        ["-j, --threads N", "Worker threads (0 = auto-detect)", "0 (auto)"],
                                        ["-q, --quiet", "Suppress statistics output", "off"],
                                        ["-s, --stats", "Print statistics to stderr", "on"],
                                        ["-H, --hwinfo", "Show hardware detection info (SIMD, cores)", "—"],
                                        ["-v, --version", "Show version", "—"],
                                        ["-h, --help", "Show help", "—"],
                                    ].map(([flag, desc, def]) => (
                                        <tr key={flag} className="border-t border-white/5">
                                            <td className="px-4 py-2 font-mono text-blue-400 whitespace-nowrap">
                                                {flag}
                                            </td>
                                            <td className="px-4 py-2">{desc}</td>
                                            <td className="px-4 py-2 text-zinc-500">{def}</td>
                                        </tr>
                                    ))}
                                </tbody>
                            </table>
                        </div>

                        <h3 className="text-lg font-semibold text-white mb-3 mt-8">
                            Examples
                        </h3>
                        <Code lang="bash">{`# Use custom patterns only
plumbr -D -p my_patterns.txt < app.log > safe.log

# Use defaults + custom patterns
plumbr -p extra_patterns.txt < app.log > safe.log

# Load all 702 patterns
plumbr -p patterns/all.txt < app.log > safe.log

# Multi-threaded (8 workers)
plumbr -j 8 < huge.log > safe.log

# CI/CD pipeline — redact before shipping logs
docker logs my-app 2>&1 | plumbr | kubectl logs-shipper

# Hardware info
plumbr -H
# AVX2: yes, SSE 4.2: yes, Cores: 12, Threads: 24`}</Code>
                    </section>

                    {/* ─── C LIBRARY ───────────────────────────────── */}
                    <section id="c-library" className="mb-20">
                        <h2 className="text-2xl font-bold text-white mb-2">C Library API</h2>
                        <p className="text-zinc-400 mb-6">
                            Embed PlumbrC directly into your C/C++ application via{" "}
                            <code className="text-blue-400 text-sm">libplumbr</code>. Available
                            as both static (<code className="text-sm">.a</code>) and shared (
                            <code className="text-sm">.so</code>) libraries.
                        </p>

                        <h3 className="text-lg font-semibold text-white mb-3">
                            Build the library
                        </h3>
                        <Code lang="bash">{`# Static library (libplumbr.a)
make lib

# Shared library (libplumbr.so)
make shared

# Files created:
# build/lib/libplumbr.a    — static
# build/lib/libplumbr.so   — shared`}</Code>

                        <h3 className="text-lg font-semibold text-white mb-3 mt-8">
                            Basic example
                        </h3>
                        <Code lang="c">{`#include <libplumbr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    // Create instance with default patterns
    libplumbr_t *p = libplumbr_new(NULL);
    if (!p) return 1;

    printf("Loaded %zu patterns\\n", libplumbr_pattern_count(p));

    // Redact a single line
    const char *input = "AWS key: AKIAIOSFODNN7EXAMPLE";
    size_t out_len;
    char *safe = libplumbr_redact(p, input, strlen(input), &out_len);

    if (safe) {
        printf("Output: %s\\n", safe);
        // Output: AWS key: [REDACTED:aws_key]
        free(safe);  // Caller owns the result
    }

    // Get statistics
    libplumbr_stats_t stats = libplumbr_get_stats(p);
    printf("Lines: %zu, Modified: %zu\\n",
           stats.lines_processed, stats.lines_modified);

    libplumbr_free(p);
    return 0;
}`}</Code>

                        <h3 className="text-lg font-semibold text-white mb-3 mt-8">
                            Compile &amp; link
                        </h3>
                        <Code lang="bash">{`# With static library
gcc -Iinclude my_app.c -Lbuild/lib -lplumbr -lpcre2-8 -lpthread -o my_app

# With shared library
gcc -Iinclude my_app.c -Lbuild/lib -lplumbr -lpcre2-8 -lpthread \\
    -Wl,-rpath,./build/lib -o my_app`}</Code>

                        <h3 className="text-lg font-semibold text-white mb-3 mt-8">
                            Configuration
                        </h3>
                        <Code lang="c">{`libplumbr_config_t config = {
    .pattern_file = "/etc/plumbr/patterns/all.txt",
    .pattern_dir  = NULL,        // Or load all files from a directory
    .num_threads  = 0,           // 0 = auto-detect
    .quiet        = 1,           // Suppress stats output
};

libplumbr_t *p = libplumbr_new(&config);`}</Code>

                        <h3 className="text-lg font-semibold text-white mb-3 mt-8">
                            In-place redaction (zero-copy)
                        </h3>
                        <Code lang="c">{`// Redact without allocating new memory
char buffer[1024];
strcpy(buffer, "token=ghp_xxxxx");
size_t len = strlen(buffer);

int new_len = libplumbr_redact_inplace(p, buffer, len, sizeof(buffer));
if (new_len >= 0) {
    buffer[new_len] = '\\0';
    printf("%s\\n", buffer);
}`}</Code>

                        <h3 className="text-lg font-semibold text-white mb-3 mt-8">
                            Batch processing
                        </h3>
                        <Code lang="c">{`const char *inputs[] = {
    "key=AKIAIOSFODNN7EXAMPLE",
    "Normal log line",
    "email: user@example.com",
};
size_t input_lens[] = { 26, 15, 23 };
char *outputs[3];
size_t output_lens[3];

int count = libplumbr_redact_batch(p, inputs, input_lens,
                                   outputs, output_lens, 3);

for (int i = 0; i < count; i++) {
    printf("%s\\n", outputs[i]);
    free(outputs[i]);
}`}</Code>

                        <h3 className="text-lg font-semibold text-white mb-3 mt-8">
                            Full API reference
                        </h3>
                        <div className="overflow-x-auto">
                            <table className="w-full text-sm border border-white/5 rounded-lg overflow-hidden">
                                <thead>
                                    <tr className="bg-white/[0.03] text-zinc-400 text-left">
                                        <th className="px-4 py-2.5 font-medium">Function</th>
                                        <th className="px-4 py-2.5 font-medium">Description</th>
                                    </tr>
                                </thead>
                                <tbody className="text-zinc-300">
                                    {[
                                        ["libplumbr_new(config)", "Create instance (NULL for defaults)"],
                                        ["libplumbr_redact(p, input, len, &out_len)", "Redact a line → new string (caller frees)"],
                                        ["libplumbr_redact_inplace(p, buf, len, cap)", "In-place redaction (zero-copy)"],
                                        ["libplumbr_redact_batch(p, ins, lens, outs, olens, n)", "Batch process multiple lines"],
                                        ["libplumbr_get_stats(p)", "Get processing statistics"],
                                        ["libplumbr_reset_stats(p)", "Reset counters to zero"],
                                        ["libplumbr_pattern_count(p)", "Number of loaded patterns"],
                                        ["libplumbr_version()", "Version string"],
                                        ["libplumbr_is_threadsafe()", "Returns 1 if thread-safe"],
                                        ["libplumbr_free(p)", "Free all resources"],
                                    ].map(([fn, desc]) => (
                                        <tr key={fn} className="border-t border-white/5">
                                            <td className="px-4 py-2 font-mono text-blue-400 text-xs whitespace-nowrap">
                                                {fn}
                                            </td>
                                            <td className="px-4 py-2">{desc}</td>
                                        </tr>
                                    ))}
                                </tbody>
                            </table>
                        </div>
                    </section>

                    {/* ─── PYTHON ──────────────────────────────────── */}
                    <section id="python" className="mb-20">
                        <h2 className="text-2xl font-bold text-white mb-2">
                            Python Package
                        </h2>
                        <p className="text-zinc-400 mb-6">
                            The <code className="text-blue-400 text-sm">plumbrc</code> package
                            wraps the C engine via ctypes — same speed, zero network overhead.{" "}
                            <strong className="text-zinc-300">83,000 lines/sec.</strong>
                        </p>

                        <h3 className="text-lg font-semibold text-white mb-3">Install</h3>
                        <Code lang="bash">{`pip install plumbrc`}</Code>

                        <h3 className="text-lg font-semibold text-white mb-3 mt-8">
                            Basic usage
                        </h3>
                        <Code lang="python">{`from plumbrc import Plumbr

# Create a redactor (loads 14 default patterns)
p = Plumbr()
print(f"Loaded {p.pattern_count} patterns")

# Redact a single line
result = p.redact("AWS key: AKIAIOSFODNN7EXAMPLE")
print(result)  # AWS key: [REDACTED:aws_key]

# Redact multiple lines
lines = [
    "User login with api_key=sk-proj-abc123",
    "Normal log line with no secrets",
    "Email sent to user@example.com",
]
safe_lines = p.redact_lines(lines)
for line in safe_lines:
    print(line)`}</Code>

                        <h3 className="text-lg font-semibold text-white mb-3 mt-8">
                            Context manager
                        </h3>
                        <Code lang="python">{`from plumbrc import Plumbr

# Automatically cleans up C resources when done
with Plumbr() as p:
    result = p.redact("password=s3cret123")
    print(result)  # password=[REDACTED:password]`}</Code>

                        <h3 className="text-lg font-semibold text-white mb-3 mt-8">
                            Custom patterns
                        </h3>
                        <Code lang="python">{`from plumbrc import Plumbr

# Load patterns from a custom file
p = Plumbr(pattern_file="/path/to/patterns.txt")

# Or load from a directory of pattern files
p = Plumbr(pattern_dir="/etc/plumbr/patterns/")`}</Code>

                        <h3 className="text-lg font-semibold text-white mb-3 mt-8">
                            Statistics
                        </h3>
                        <Code lang="python">{`p = Plumbr()

# Process some text
for i in range(1000):
    p.redact(f"api_key=AKIAIOSFODNN7EXAMPLE line {i}")

# Check stats
stats = p.stats
print(f"Lines processed: {stats['lines_processed']}")
print(f"Lines modified:  {stats['lines_modified']}")
print(f"Patterns matched: {stats['patterns_matched']}")

# Get version
print(f"PlumbrC version: {Plumbr.version()}")`}</Code>

                        <h3 className="text-lg font-semibold text-white mb-3 mt-8">
                            Process log files
                        </h3>
                        <Code lang="python">{`from plumbrc import Plumbr

p = Plumbr()

# Redact an entire log file
with open("app.log") as f_in, open("safe.log", "w") as f_out:
    for line in f_in:
        f_out.write(p.redact(line))

print(f"Done. Stats: {p.stats}")`}</Code>

                        <h3 className="text-lg font-semibold text-white mb-3 mt-8">
                            Error handling
                        </h3>
                        <Code lang="python">{`from plumbrc import Plumbr, LibraryNotFoundError, RedactionError

try:
    p = Plumbr()
except LibraryNotFoundError:
    print("libplumbr.so not found — install PlumbrC first")

try:
    result = p.redact(some_text)
except RedactionError as e:
    print(f"Redaction failed: {e}")`}</Code>
                    </section>

                    {/* ─── REST API ────────────────────────────────── */}
                    <section id="rest-api" className="mb-20">
                        <h2 className="text-2xl font-bold text-white mb-2">REST API</h2>
                        <p className="text-zinc-400 mb-2">
                            Base URL:{" "}
                            <code className="text-blue-400 text-sm">
                                https://plumbr.ca/api
                            </code>
                        </p>
                        <div className="mb-6 p-3 rounded-lg bg-amber-500/5 border border-amber-500/10 text-sm text-amber-400/80">
                            <strong className="text-amber-400">Honest note:</strong> The C
                            engine processes each request in ~0.1ms. But network round-trip
                            adds 100–800ms depending on your location. The REST API is best for
                            testing and prototyping. For production throughput, use the native C
                            binary or Python package locally.
                        </div>

                        <h3 className="text-lg font-semibold text-white mb-3">
                            POST /api/redact
                        </h3>
                        <p className="text-zinc-400 text-sm mb-3">
                            Redact secrets from a text string. Handles multi-line input.
                        </p>
                        <Code lang="bash">{`curl -X POST https://plumbr.ca/api/redact \\
  -H "Content-Type: application/json" \\
  -d '{"text": "AWS key: AKIAIOSFODNN7EXAMPLE\\nemail: user@example.com"}'`}</Code>
                        <Code lang="json">{`{
  "redacted": "AWS key: [REDACTED:aws_key]\\nemail: [REDACTED:email]",
  "stats": {
    "lines_processed": 2,
    "lines_modified": 2,
    "patterns_matched": 2,
    "processing_time_ms": 0.066
  }
}`}</Code>

                        <h3 className="text-lg font-semibold text-white mb-3 mt-8">
                            POST /api/redact/batch
                        </h3>
                        <p className="text-zinc-400 text-sm mb-3">
                            Process multiple texts in a single request. More efficient for bulk
                            operations.
                        </p>
                        <Code lang="bash">{`curl -X POST https://plumbr.ca/api/redact/batch \\
  -H "Content-Type: application/json" \\
  -d '{
    "texts": [
      "key=AKIAIOSFODNN7EXAMPLE",
      "Normal log line",
      "token=ghp_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
    ]
  }'`}</Code>

                        <h3 className="text-lg font-semibold text-white mb-3 mt-8">
                            GET /health
                        </h3>
                        <p className="text-zinc-400 text-sm mb-3">
                            Health check and server statistics.
                        </p>
                        <Code lang="bash">{`curl https://plumbr.ca/health`}</Code>
                        <Code lang="json">{`{
  "status": "healthy",
  "version": "1.0.2",
  "server_version": "1.0.0",
  "uptime_seconds": 3600.0,
  "patterns_loaded": 14,
  "stats": {
    "requests_total": 1523,
    "requests_ok": 1520,
    "requests_error": 3,
    "bytes_processed": 524288,
    "avg_rps": 0.4
  }
}`}</Code>

                        <h3 className="text-lg font-semibold text-white mb-3 mt-8">
                            Self-host the API server
                        </h3>
                        <Code lang="bash">{`# Build the server
make server

# Run on custom port
./build/bin/plumbr-server --port 9090 --threads 4

# With all patterns
./build/bin/plumbr-server --pattern-file patterns/all.txt

# Server options:
#   --port PORT         Listen port (default: 8080)
#   --host ADDR         Bind address (default: 0.0.0.0)
#   --threads N         Worker threads (0=auto)
#   --pattern-dir DIR   Load patterns from directory
#   --pattern-file FILE Load patterns from file`}</Code>
                    </section>

                    {/* ─── PATTERNS ────────────────────────────────── */}
                    <section id="patterns" className="mb-20">
                        <h2 className="text-2xl font-bold text-white mb-2">
                            Custom Patterns
                        </h2>
                        <p className="text-zinc-400 mb-6">
                            PlumbrC ships with 702 patterns in 12 categories. You can also
                            define your own.
                        </p>

                        <h3 className="text-lg font-semibold text-white mb-3">
                            Pattern file format
                        </h3>
                        <p className="text-zinc-400 text-sm mb-3">
                            One pattern per line. Four pipe-delimited fields:
                        </p>
                        <Code lang="text">{`name|literal|regex|replacement

# Fields:
#   name        — Human-readable label (used in [REDACTED:name])
#   literal     — Literal prefix for Aho-Corasick (fast pre-filter)
#   regex       — PCRE2 regex for exact matching
#   replacement — Replacement text (usually [REDACTED:name])`}</Code>

                        <h3 className="text-lg font-semibold text-white mb-3 mt-8">
                            Example patterns
                        </h3>
                        <Code lang="text">{`# AWS Access Key
aws_key|AKIA|AKIA[0-9A-Z]{16}|[REDACTED:aws_key]

# GitHub Personal Access Token
github_pat|ghp_|ghp_[A-Za-z0-9]{36}|[REDACTED:github_pat]

# Email address
email|@|[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}|[REDACTED:email]

# Custom API key for your service
my_api|MYKEY_|MYKEY_[a-f0-9]{32}|[REDACTED:my_api_key]`}</Code>

                        <h3 className="text-lg font-semibold text-white mb-3 mt-8">
                            Built-in categories
                        </h3>
                        <div className="grid grid-cols-2 sm:grid-cols-3 gap-2">
                            {[
                                ["cloud", "AWS, GCP, Azure, DigitalOcean"],
                                ["auth", "OAuth, JWT, session tokens"],
                                ["vcs", "GitHub, GitLab, Bitbucket"],
                                ["payment", "Stripe, PayPal, Square"],
                                ["database", "PostgreSQL, MySQL, MongoDB"],
                                ["crypto", "Private keys, mnemonics"],
                                ["pii", "Email, phone, SSN, IP"],
                                ["secrets", "Generic API keys, passwords"],
                                ["infra", "Docker, K8s, Terraform"],
                                ["communication", "Slack, Twilio, SendGrid"],
                                ["analytics", "Mixpanel, Segment, Amplitude"],
                                ["social", "Facebook, Twitter tokens"],
                            ].map(([cat, desc]) => (
                                <div
                                    key={cat}
                                    className="p-3 rounded-lg bg-white/[0.02] border border-white/5 text-sm"
                                >
                                    <div className="font-mono text-blue-400 text-xs">{cat}/</div>
                                    <div className="text-zinc-500 text-xs mt-0.5">{desc}</div>
                                </div>
                            ))}
                        </div>

                        <h3 className="text-lg font-semibold text-white mb-3 mt-8">
                            Loading patterns
                        </h3>
                        <Code lang="bash">{`# Default 14 patterns (built-in, no file needed)
plumbr < app.log > safe.log

# Load all 702 patterns
plumbr -p patterns/all.txt < app.log > safe.log

# Specific category
plumbr -p patterns/cloud/aws.txt < app.log > safe.log

# Multiple: defaults + custom
plumbr -p extra.txt < app.log > safe.log

# Custom only (disable defaults)
plumbr -D -p my_patterns.txt < app.log > safe.log`}</Code>
                    </section>

                    {/* ─── INTEGRATIONS ────────────────────────────── */}
                    <section id="integrations" className="mb-20">
                        <h2 className="text-2xl font-bold text-white mb-2">
                            Integrations
                        </h2>
                        <p className="text-zinc-400 mb-6">
                            Ready-to-use configs for Docker, Kubernetes, and popular log
                            pipelines.
                        </p>

                        <h3 className="text-lg font-semibold text-white mb-3">Docker</h3>
                        <Code lang="yaml">{`# docker-compose.yml
version: '3.8'
services:
  app:
    image: your-app:latest
    volumes:
      - app-logs:/var/log/app

  plumbr:
    build:
      context: .
      dockerfile: Dockerfile
    entrypoint: >
      sh -c "tail -F /var/log/app/*.log | plumbr > /var/log/safe/app.log"
    volumes:
      - app-logs:/var/log/app:ro
      - safe-logs:/var/log/safe
    restart: unless-stopped

volumes:
  app-logs:
  safe-logs:`}</Code>

                        <h3 className="text-lg font-semibold text-white mb-3 mt-8">
                            Kubernetes sidecar
                        </h3>
                        <Code lang="yaml">{`# Your app + PlumbrC sidecar in the same pod
containers:
  - name: app
    image: your-app:latest
    volumeMounts:
      - name: logs
        mountPath: /var/log/app

  - name: plumbr
    image: plumbrc:latest
    command: ["sh", "-c", "tail -F /var/log/app/*.log | plumbr > /var/log/safe/app.log"]
    volumeMounts:
      - name: logs
        mountPath: /var/log/app
        readOnly: true
      - name: safe-logs
        mountPath: /var/log/safe
    resources:
      requests: { cpu: 50m, memory: 32Mi }
      limits: { cpu: 200m, memory: 64Mi }`}</Code>

                        <h3 className="text-lg font-semibold text-white mb-3 mt-8">
                            systemd service
                        </h3>
                        <Code lang="ini">{`[Unit]
Description=PlumbrC Log Redaction Server
After=network.target

[Service]
ExecStart=/usr/local/bin/plumbr-server --port 8080
Restart=always
User=plumbr

[Install]
WantedBy=multi-user.target`}</Code>

                        <h3 className="text-lg font-semibold text-white mb-3 mt-8">
                            Other pipelines
                        </h3>
                        <div className="space-y-3 text-sm text-zinc-400">
                            <p>
                                <strong className="text-zinc-300">Fluentd:</strong>{" "}
                                Use the exec filter to pipe logs through plumbr. Config in{" "}
                                <code className="text-blue-400">integrations/fluentd/</code>
                            </p>
                            <p>
                                <strong className="text-zinc-300">Logstash:</strong>{" "}
                                Use the pipe filter. Config in{" "}
                                <code className="text-blue-400">integrations/logstash/</code>
                            </p>
                            <p>
                                <strong className="text-zinc-300">Vector:</strong>{" "}
                                Use the exec transform. Config in{" "}
                                <code className="text-blue-400">integrations/vector/</code>
                            </p>
                        </div>
                    </section>

                    {/* ─── BUILDING FROM SOURCE ────────────────────── */}
                    <section id="building" className="mb-20">
                        <h2 className="text-2xl font-bold text-white mb-2">
                            Building from Source
                        </h2>
                        <p className="text-zinc-400 mb-6">
                            PlumbrC is pure C11 with minimal dependencies.
                        </p>

                        <h3 className="text-lg font-semibold text-white mb-3">
                            Prerequisites
                        </h3>
                        <Code lang="bash">{`# Ubuntu/Debian
sudo apt install build-essential libpcre2-dev

# Fedora/RHEL
sudo dnf install gcc make pcre2-devel

# macOS (x86_64 only — SIMD requires x86)
brew install pcre2`}</Code>

                        <h3 className="text-lg font-semibold text-white mb-3 mt-8">
                            Build targets
                        </h3>
                        <div className="overflow-x-auto">
                            <table className="w-full text-sm border border-white/5 rounded-lg overflow-hidden">
                                <thead>
                                    <tr className="bg-white/[0.03] text-zinc-400 text-left">
                                        <th className="px-4 py-2.5 font-medium">Command</th>
                                        <th className="px-4 py-2.5 font-medium">Output</th>
                                        <th className="px-4 py-2.5 font-medium">Description</th>
                                    </tr>
                                </thead>
                                <tbody className="text-zinc-300">
                                    {[
                                        ["make", "build/bin/plumbr", "Optimized release binary (-O3, LTO, -march=native)"],
                                        ["make debug", "build/bin/plumbr", "Debug build (-g, -O0)"],
                                        ["make server", "build/bin/plumbr-server", "HTTP API server"],
                                        ["make lib", "build/lib/libplumbr.a", "Static library"],
                                        ["make shared", "build/lib/libplumbr.so", "Shared library (for Python)"],
                                        ["make sanitize", "build/bin/plumbr", "ASan + UBSan build"],
                                        ["make install", "—", "Install to /usr/local/bin"],
                                        ["make clean", "—", "Remove all build artifacts"],
                                    ].map(([cmd, out, desc]) => (
                                        <tr key={cmd} className="border-t border-white/5">
                                            <td className="px-4 py-2 font-mono text-blue-400 whitespace-nowrap">
                                                {cmd}
                                            </td>
                                            <td className="px-4 py-2 font-mono text-zinc-500 text-xs whitespace-nowrap">
                                                {out}
                                            </td>
                                            <td className="px-4 py-2">{desc}</td>
                                        </tr>
                                    ))}
                                </tbody>
                            </table>
                        </div>

                        <h3 className="text-lg font-semibold text-white mb-3 mt-8">
                            Quick install script
                        </h3>
                        <Code lang="bash">{`# One-liner install
curl -sSL https://raw.githubusercontent.com/AmritRai1234/plumbrC/main/install.sh | bash

# Or manually:
git clone https://github.com/AmritRai1234/plumbrC.git
cd plumbrC
make -j$(nproc)
sudo make install
plumbr --version`}</Code>
                    </section>

                    {/* ─── PERFORMANCE ─────────────────────────────── */}
                    <section id="performance" className="mb-20">
                        <h2 className="text-2xl font-bold text-white mb-2">Performance</h2>
                        <p className="text-zinc-400 mb-6">
                            Real benchmark numbers on AMD Ryzen 5 5600X. Optimized for AMD with
                            AVX2 SIMD.
                        </p>

                        <h3 className="text-lg font-semibold text-white mb-3">
                            Native C binary
                        </h3>
                        <div className="overflow-x-auto">
                            <table className="w-full text-sm border border-white/5 rounded-lg overflow-hidden">
                                <thead>
                                    <tr className="bg-white/[0.03] text-zinc-400 text-left">
                                        <th className="px-4 py-2.5 font-medium">Config</th>
                                        <th className="px-4 py-2.5 font-medium">Single-threaded</th>
                                        <th className="px-4 py-2.5 font-medium">Multi-threaded (8T)</th>
                                    </tr>
                                </thead>
                                <tbody className="text-zinc-300">
                                    <tr className="border-t border-white/5">
                                        <td className="px-4 py-2 font-medium">Default (14 patterns)</td>
                                        <td className="px-4 py-2 text-emerald-400 font-mono">
                                            3.7M lines/sec
                                        </td>
                                        <td className="px-4 py-2 text-emerald-400 font-mono">
                                            4.9M lines/sec
                                        </td>
                                    </tr>
                                    <tr className="border-t border-white/5">
                                        <td className="px-4 py-2 font-medium">Full (702 patterns)</td>
                                        <td className="px-4 py-2 text-emerald-400 font-mono">
                                            1.1M lines/sec
                                        </td>
                                        <td className="px-4 py-2 text-emerald-400 font-mono">
                                            2.6M lines/sec
                                        </td>
                                    </tr>
                                </tbody>
                            </table>
                        </div>

                        <h3 className="text-lg font-semibold text-white mb-3 mt-8">
                            Comparison
                        </h3>
                        <div className="overflow-x-auto">
                            <table className="w-full text-sm border border-white/5 rounded-lg overflow-hidden">
                                <thead>
                                    <tr className="bg-white/[0.03] text-zinc-400 text-left">
                                        <th className="px-4 py-2.5 font-medium">Method</th>
                                        <th className="px-4 py-2.5 font-medium">Throughput</th>
                                        <th className="px-4 py-2.5 font-medium">Bottleneck</th>
                                    </tr>
                                </thead>
                                <tbody className="text-zinc-300">
                                    {[
                                        [
                                            "Native C binary",
                                            "5M lines/sec",
                                            "CPU (AVX2 + SSE 4.2)",
                                        ],
                                        [
                                            "Python package (ctypes → C)",
                                            "83K lines/sec",
                                            "FFI overhead (~12μs/call)",
                                        ],
                                        [
                                            "REST API (over internet)",
                                            "~0.1ms server-side",
                                            "Network latency (100-800ms RTT)",
                                        ],
                                    ].map(([method, speed, bottleneck]) => (
                                        <tr key={method} className="border-t border-white/5">
                                            <td className="px-4 py-2 font-medium">{method}</td>
                                            <td className="px-4 py-2 text-emerald-400 font-mono">
                                                {speed}
                                            </td>
                                            <td className="px-4 py-2 text-zinc-500">{bottleneck}</td>
                                        </tr>
                                    ))}
                                </tbody>
                            </table>
                        </div>

                        <h3 className="text-lg font-semibold text-white mb-3 mt-8">
                            Why it&apos;s fast
                        </h3>
                        <div className="space-y-3 text-sm text-zinc-400">
                            <div className="flex gap-3">
                                <Shield size={16} className="text-blue-400 mt-0.5 shrink-0" />
                                <div>
                                    <strong className="text-zinc-300">
                                        SSE 4.2 hardware pre-filter
                                    </strong>{" "}
                                    — Scans 16 bytes/cycle for trigger characters. Skips 97% of clean
                                    lines before any regex runs.
                                </div>
                            </div>
                            <div className="flex gap-3">
                                <Zap size={16} className="text-blue-400 mt-0.5 shrink-0" />
                                <div>
                                    <strong className="text-zinc-300">
                                        Aho-Corasick DFA
                                    </strong>{" "}
                                    — All 702 patterns matched in a single O(n) pass. No backtracking,
                                    constant time regardless of pattern count.
                                </div>
                            </div>
                            <div className="flex gap-3">
                                <Cpu size={16} className="text-blue-400 mt-0.5 shrink-0" />
                                <div>
                                    <strong className="text-zinc-300">PCRE2 JIT</strong> — Only fires
                                    on the ~5% of lines with an Aho-Corasick hit. JIT-compiled
                                    regexes with ReDoS protection (match limits).
                                </div>
                            </div>
                            <div className="flex gap-3">
                                <Server size={16} className="text-blue-400 mt-0.5 shrink-0" />
                                <div>
                                    <strong className="text-zinc-300">Zero heap allocations</strong>{" "}
                                    — Arena allocator for all memory. No malloc in the hot path. Zero
                                    GC pauses.
                                </div>
                            </div>
                        </div>
                    </section>

                    {/* ─── Footer ─────────────────────────────────── */}
                    <footer className="pt-10 border-t border-white/5 text-sm text-zinc-500 flex justify-between">
                        <span>PlumbrC — Open Source · MIT License</span>
                        <a
                            href="https://github.com/AmritRai1234/plumbrC"
                            className="text-zinc-400 hover:text-white transition-colors"
                        >
                            GitHub →
                        </a>
                    </footer>
                </main>
            </div>
        </div>
    );
}
