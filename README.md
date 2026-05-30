<p align="center">
  <h1 align="center">PlumbrC</h1>
  <p align="center"><strong>High-Performance Log Redaction Engine</strong></p>
  <p align="center">C11 &bull; 8.5M lines/sec &bull; Zero-alloc hot path &bull; Rust & Go bindings</p>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/language-C11-blue.svg" alt="Language">
  <img src="https://img.shields.io/badge/performance-8.5M%20lines%2Fsec-brightgreen.svg" alt="Performance">
  <img src="https://img.shields.io/badge/throughput-798%20MB%2Fs-blue.svg" alt="Throughput">
  <img src="https://img.shields.io/badge/GPU-OpenCL-yellow.svg" alt="GPU">
  <img src="https://img.shields.io/badge/memory-0%20allocs%20hot%20path-success.svg" alt="Memory">
  <img src="https://img.shields.io/badge/license-MIT-green.svg" alt="License">
</p>

---

PlumbrC is a C11 library that detects and redacts secrets from text streams at extreme speed. A three-phase matching cascade — **SSE4.2 sentinel scan → Aho-Corasick DFA → PCRE2 JIT regex** — achieves **8.5M lines/sec** (median, with warmup) on commodity hardware with zero heap allocations on the hot path.

Use it as a **C library**, a **Rust crate**, or a **Go package**.

## Table of Contents

- [Quick Start](#quick-start)
- [Installation](#installation)
- [API Reference](#api-reference)
  - [C](#c-api)
  - [Rust](#rust)
  - [Go](#go)
- [Architecture](#architecture)
- [Benchmarks](#benchmarks)
- [Pattern Library](#pattern-library)
- [GPU Acceleration](#gpu-acceleration)
- [Build Targets](#build-targets)
- [License](#license)

## Quick Start

### C
```c
#include <plumbr/libplumbr.h>

int main(void) {
    libplumbr_t *p = libplumbr_new(NULL);

    // Allocating — returns a new string
    size_t out_len;
    char *safe = libplumbr_redact(p, "password=secret123", 18, &out_len);
    printf("%s\n", safe);  // "password=[REDACTED:password]"
    libplumbr_free_string(safe);

    // Zero-alloc — writes into your buffer
    char buf[256];
    ssize_t n = libplumbr_redact_into(p, "api_key=AKIA...", 15, buf, sizeof(buf));
    if (n >= 0) printf("%.*s\n", (int)n, buf);

    // Check errors
    if (n < 0) {
        printf("Error: %s\n", libplumbr_error_string(libplumbr_last_error()));
    }

    libplumbr_free(p);
}
```

### Rust
```rust
use plumbr::Plumbr;

fn main() {
    let p = Plumbr::new().unwrap();

    // Simple string redaction
    let safe = p.redact("api_key=AKIAIOSFODNN7EXAMPLE");
    assert!(!safe.contains("AKIAIOSFODNN7EXAMPLE"));

    // Zero-alloc into caller buffer
    let mut buf = [0u8; 256];
    let n = p.redact_into(b"password=secret123", &mut buf).unwrap();

    // Batch processing
    let results = p.redact_batch(&["line1", "key=secret", "line3"]);

    // Stats
    let stats = p.stats();
    println!("{} lines, {} modified", stats.lines_processed, stats.lines_modified);
}
```

### Go
```go
package main

import (
    "fmt"
    plumbr "github.com/AmritRai1234/plumbrC/bindings/go"
)

func main() {
    p, err := plumbr.New(nil)
    if err != nil {
        panic(err)
    }
    defer p.Close()

    // Simple string redaction
    safe := p.Redact("api_key=AKIAIOSFODNN7EXAMPLE")
    fmt.Println(safe)

    // Zero-alloc into caller buffer
    output := make([]byte, 256)
    n, _ := p.RedactInto([]byte("password=secret"), output)
    fmt.Println(string(output[:n]))

    // Batch processing
    results := p.RedactBatch([]string{"line1", "key=secret", "line3"})

    // Stats
    stats := p.Stats()
    fmt.Printf("%d lines, %d modified\n", stats.LinesProcessed, stats.LinesModified)
}
```

## Installation

### Prerequisites

```bash
sudo apt install build-essential libpcre2-dev    # Debian/Ubuntu
brew install pcre2                                # macOS
```

### Build from source

```bash
git clone https://github.com/AmritRai1234/plumbrC.git
cd plumbrC

make lib           # Static library  → build/lib/libplumbr.a
make shared        # Shared library  → build/lib/libplumbr.so
make test          # Run all tests (48 tests across 5 suites)
make benchmark-full  # Run benchmarks (median of 3 runs with warmup)
sudo make install  # Install to /usr/local
```

### Linking

```bash
# Static
gcc myapp.c -lplumbr -lpcre2-8 -lpthread -o myapp

# Shared
gcc myapp.c -L/usr/local/lib -lplumbr -lpcre2-8 -lpthread -o myapp
```

### Rust

```bash
cd bindings/rust
cargo test     # Build + run 10 tests
cargo bench    # Benchmark
```

Add to your project:
```toml
[dependencies]
plumbr = { path = "path/to/plumbrC/bindings/rust" }
```

### Go

Requires the C library to be built first (`make lib`):

```bash
cd bindings/go
go test -v       # Run tests
go test -bench .  # Benchmark
```

## API Reference

### C API

```c
/* ── Lifecycle ── */
libplumbr_t *libplumbr_new(const libplumbr_config_t *config);  // NULL = defaults
void         libplumbr_free(libplumbr_t *p);

/* ── Redaction ── */
// Allocating — returns new string, caller frees with libplumbr_free_string()
char *libplumbr_redact(libplumbr_t *p, const char *input, size_t len, size_t *out_len);

// Zero-alloc — writes into caller buffer, returns bytes written or negative error
ssize_t libplumbr_redact_into(libplumbr_t *p, const char *input, size_t len,
                              char *output, size_t capacity);

// In-place — redacts directly in the buffer
ssize_t libplumbr_redact_inplace(libplumbr_t *p, char *buf, size_t len, size_t cap);

// Batch — redacts multiple lines
int libplumbr_redact_batch(libplumbr_t *p, const char **inputs,
                           const size_t *lens, char **outputs,
                           size_t *out_lens, size_t count);

// Bulk — redacts newline-separated buffer in one call (ideal for FFI)
char *libplumbr_redact_buffer(libplumbr_t *p, const char *input,
                              size_t len, size_t *out_len);

/* ── Memory ── */
void libplumbr_free_string(char *str);   // Free strings returned by redact/buffer

/* ── Error Handling ── */
libplumbr_error_t libplumbr_last_error(void);           // Thread-local error code
const char       *libplumbr_error_string(libplumbr_error_t err);  // Human-readable

/* ── Info ── */
libplumbr_stats_t libplumbr_get_stats(const libplumbr_t *p);
size_t            libplumbr_pattern_count(const libplumbr_t *p);
const char       *libplumbr_version(void);
```

#### Error Codes

| Code | Constant | Meaning |
|------|----------|---------|
| `0`  | `PLUMBR_OK` | Success |
| `-1` | `PLUMBR_ERR_ALLOC` | Memory allocation failed |
| `-2` | `PLUMBR_ERR_PATTERN` | Invalid pattern or pattern file |
| `-3` | `PLUMBR_ERR_INPUT_TOO_LARGE` | Input exceeds 64KB max line size |
| `-4` | `PLUMBR_ERR_BUFFER_TOO_SMALL` | Output buffer too small |
| `-5` | `PLUMBR_ERR_NULL_INPUT` | NULL pointer argument |

#### Configuration

```c
libplumbr_config_t cfg = {
    .pattern_file = "patterns/custom.txt",  // Custom patterns (NULL = defaults)
    .pattern_dir  = "patterns/extra/",      // Load all .txt files in directory
    .compliance   = "hipaa,pci",            // Compliance profiles
    .num_threads  = 0,                      // 0 = auto-detect optimal count
    .quiet        = 1                       // Suppress stats output
};
libplumbr_t *p = libplumbr_new(&cfg);
```

#### Thread Safety

Each `libplumbr_t` instance must be used from a **single thread**. For concurrent redaction, create one instance per thread. Error codes (`libplumbr_last_error`) are thread-local.

### Rust

| Function | Description |
|----------|-------------|
| `Plumbr::new()` | Create with default patterns |
| `Plumbr::with_config(cfg)` | Create with custom config |
| `p.redact(str)` | Redact a `&str` → `String` |
| `p.redact_bytes(bytes)` | Redact `&[u8]` → `Vec<u8>` |
| `p.redact_into(input, buf)` | Zero-alloc into `&mut [u8]` → `Result<usize>` |
| `p.redact_batch(strs)` | Batch redact `&[&str]` → `Vec<String>` |
| `p.redact_buffer(bytes)` | Bulk redact newline-separated → `Vec<u8>` |
| `p.stats()` | Get `Stats { lines_processed, ... }` |
| `p.pattern_count()` | Number of loaded patterns |
| `Plumbr::version()` | Library version string |

### Go

| Function | Description |
|----------|-------------|
| `plumbr.New(cfg)` | Create instance (`nil` = defaults) |
| `p.Close()` | Free resources (also called by finalizer) |
| `p.Redact(str)` | Redact `string` → `string` |
| `p.RedactBytes([]byte)` | Redact `[]byte` → `([]byte, error)` |
| `p.RedactInto(in, out)` | Zero-alloc into `[]byte` → `(int, error)` |
| `p.RedactBatch([]string)` | Batch redact → `[]string` |
| `p.RedactBuffer([]byte)` | Bulk redact newline-separated → `([]byte, error)` |
| `p.Stats()` | Get `Stats{LinesProcessed, ...}` |
| `p.PatternCount()` | Number of loaded patterns |
| `plumbr.Version()` | Library version string |

## Architecture

```
Input ──→ SSE4.2 Sentinel ──→ Aho-Corasick DFA ──→ PCRE2 JIT ──→ Output
            │ ~1 ns/line        │ ~15 ns/line        │ ~200 ns/line
            │ 90% rejected      │ 95% rejected       │ Confirms match
            ▼                   ▼                    ▼
         Fast path          Pattern match         Regex verify
```

### Why it's fast

| Layer | What it does | Speed |
|-------|-------------|-------|
| **SSE4.2 sentinel** | `PCMPISTRI` scans 16 bytes/cycle for trigger characters (`=`, `"`, `'`) | ~1 ns/line |
| **Aho-Corasick DFA** | Multi-pattern matching with bitmap-compressed sparse rows | ~15 ns/line |
| **PCRE2 JIT** | Native-code regex with JIT compilation for verification | ~200 ns/match |
| **Arena allocator** | Zero `malloc` on hot path — all scratch memory pre-allocated (16MB) | 0 allocs |
| **Contiguous pools** | Batch buffers are 2 contiguous blocks, not 16K individual mallocs | Better TLB |
| **Auto-tuned threads** | CPU-aware thread count (1.75× physical cores on Zen 3/4) | Max throughput |
| **GPU pre-alloc** | GPU batch buffers allocated once at init, not per-batch | 0 allocs on GPU path |

### Performance cascade

90% of log lines contain no secrets. The SSE4.2 sentinel scan rejects them in **~1 nanosecond** — they never touch regex. This is why PlumbrC is 25,000× faster per clean line than tools that run every regex against every line.

```
1M lines → SSE4.2 → 100K candidates → AC DFA → 50K matches → PCRE2 → 10K redacted
           (900K skipped in ~1ns each)
```

## Benchmarks

Median of 3 measured runs with 1 warmup run. AMD Zen 3 (Ryzen 5000, 8C/16T).

| Scenario | Throughput | MB/s | Config |
|----------|-----------|------|--------|
| 5M enterprise (5% secrets) | **8.53M lines/sec** | 798 | auto (14 threads) |
| 1M clean lines | **7.70M lines/sec** | 720 | auto (14 threads) |
| 1M 10% secrets | 7.21M lines/sec | 676 | auto (14 threads) |
| 1M 100% secrets | 5.52M lines/sec | 528 | auto (14 threads) |
| 1M clean (single thread) | 3.15M lines/sec | 294 | 1 thread |
| 5M enterprise (single thread) | 3.02M lines/sec | 282 | 1 thread |

### vs Other Tools

| Tool | Language | Speed | Does Redaction? |
|------|----------|-------|-----------------|
| **PlumbrC** | C11 | **8.53M l/s** | ✅ Yes |
| ripgrep | Rust | ~8M l/s | ❌ Search only |
| Hyperscan | C | ~7-10M l/s | ❌ Match only |
| gitleaks | Go | ~1-2M l/s | ❌ Detect only |
| trufflehog | Go | ~500K l/s | ❌ Detect + verify |
| detect-secrets | Python | ~100-200K l/s | ❌ Detect only |
| Presidio | Python | ~10-50K l/s | ✅ Yes (ML-based) |

## Pattern Library

14 built-in patterns covering common secret types:

| Category | Patterns |
|----------|----------|
| **Credentials** | AWS access keys (`AKIA...`), passwords, generic secrets |
| **Tokens** | GitHub PATs (`ghp_`), JWTs (`eyJ`), API keys |
| **Keys** | RSA/EC private keys (`-----BEGIN`) |
| **PII** | Email addresses, SSN, credit cards, IPv4 addresses |
| **Infrastructure** | Database connection URIs (`postgres://`, `mysql://`) |

### Compliance Profiles

```c
libplumbr_config_t cfg = { .compliance = "hipaa,pci" };
```

| Profile | Patterns | Coverage |
|---------|----------|----------|
| `hipaa` | 27 | MRN, NPI, ICD-10, DEA numbers, insurance IDs |
| `pci` | 16 | Track data, PIN blocks, PAN, CVV, magnetic stripe |
| `gdpr` | 23 | IBAN, EU national IDs, VAT numbers |
| `soc2` | 25 | Audit logs, access tokens, session IDs |
| `all` | 91 | All of the above |

## GPU Acceleration

Optional OpenCL GPU acceleration for Aho-Corasick DFA scanning. Works with **AMD, NVIDIA, and Intel GPUs** — any OpenCL 1.2+ device.

```bash
# Install OpenCL
sudo apt install ocl-icd-opencl-dev opencl-headers
sudo apt install mesa-opencl-icd     # AMD (open-source)

# Build with GPU support
make gpu

# Run GPU benchmarks
make gpu-benchmark

# Force CPU-only at runtime
PLUMBR_NO_GPU=1 ./your_app
```

The GPU path uses **zero hot-path allocations** — all batch buffers are pre-allocated at init. The DFA is uploaded once to GPU memory. Lines with no AC matches bypass PCRE2 entirely (passthrough).

## Build Targets

```bash
make lib            # Static library (libplumbr.a)
make shared         # Shared library (libplumbr.so)
make libs           # Both static and shared
make gpu            # Build with OpenCL GPU acceleration
make test           # Unit tests (48 tests across 5 suites)
make gpu-test       # Unit tests with GPU enabled
make sanitize       # Build with ASan + UBSan
make benchmark-full # Performance benchmarks (median of 3 runs)
make gpu-benchmark  # GPU performance benchmarks
make fuzz           # Build libFuzzer harnesses
make install        # Install to /usr/local
make clean          # Remove build artifacts
```

## Project Structure

```
plumbrC/
├── include/            # Public headers
│   ├── libplumbr.h     # ← Library API (use this)
│   ├── config.h        # Constants and limits
│   └── ...
├── src/                # C implementation
│   ├── libplumbr.c     # Library API implementation
│   ├── pipeline.c      # Multi-threaded processing pipeline
│   ├── aho_corasick.c  # Aho-Corasick DFA (833 lines)
│   ├── redactor.c      # Pattern matching + redaction
│   ├── parallel.c      # pthread barrier fork-join
│   ├── arena.c         # Bump allocator (mmap-backed)
│   ├── hwdetect.c      # CPU detection + tuning
│   ├── amd/            # SSE4.2 + AVX2 SIMD
│   └── gpu/            # OpenCL kernel + host code
├── bindings/
│   ├── rust/           # Rust crate (cargo test)
│   │   ├── src/lib.rs  # Safe wrapper
│   │   └── src/ffi.rs  # Raw C FFI
│   └── go/             # Go package (go test)
│       ├── plumbr.go   # cgo wrapper
│       └── plumbr_test.go
├── tests/              # Test suites
│   ├── test_libplumbr.c  # Library API tests (13 tests)
│   ├── test_redactor.c   # Redactor tests (6 tests)
│   ├── test_security.c   # Security regression (10 tests)
│   ├── test_io.c         # I/O tests (8 tests)
│   └── benchmark.c       # Benchmark suite
└── patterns/           # Pattern files + compliance
```

## License

MIT License. See [LICENSE](LICENSE).

## Contributing

1. Fork the repository
2. Create a feature branch
3. Run `make sanitize && make test` before submitting
4. Open a PR

---

<p align="center"><strong>PlumbrC</strong> — The fastest real-time log redaction engine.</p>
