<p align="center">
  <h1 align="center">PlumbrC</h1>
  <p align="center"><strong>High-Performance Log Redaction Library</strong></p>
  <p align="center">Pure C11 &bull; 9.2M lines/sec &bull; Zero-allocation hot path &bull; 14 built-in patterns</p>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/language-C11-blue.svg" alt="Language">
  <img src="https://img.shields.io/badge/performance-9.2M%20lines%2Fsec-brightgreen.svg" alt="Performance">
  <img src="https://img.shields.io/badge/throughput-860%20MB%2Fs-blue.svg" alt="Throughput">
  <img src="https://img.shields.io/badge/GPU-OpenCL-yellow.svg" alt="GPU">
  <img src="https://img.shields.io/badge/memory-0%20allocs%20hot%20path-success.svg" alt="Memory">
  <img src="https://img.shields.io/badge/license-Source%20Available-orange.svg" alt="License">
</p>

---

PlumbrC is a C11 library that detects and removes secrets from text at extreme speed. Three-phase matching — SSE4.2 sentinel scan → Aho-Corasick literal DFA → PCRE2 JIT regex — achieves **9.2M lines/sec** on commodity hardware with zero heap allocations on the hot path.

## Build

```bash
# Dependencies
sudo apt install build-essential libpcre2-dev

# Build static library (libplumbr.a)
make

# Build with GPU acceleration (requires OpenCL)
sudo apt install ocl-icd-opencl-dev opencl-headers
make gpu

# Build shared library (libplumbr.so)
make shared

# Build both
make libs

# Run tests (45 unit tests)
make test

# Run benchmarks
make benchmark-full

# Install to /usr/local
sudo make install
```

## Usage

```c
#include <plumbr/libplumbr.h>

int main(void) {
    // Create with default patterns
    libplumbr_t *p = libplumbr_new(NULL);

    // Redact a single line
    char *safe = libplumbr_redact(p, "password=secret123", 18, NULL);
    printf("%s\n", safe);  // "password=[REDACTED:password]"
    libplumbr_free_string(safe);

    // Bulk redact
    size_t out_len;
    char *result = libplumbr_redact_buffer(p, input, input_len, &out_len);
    libplumbr_free_string(result);

    // Cleanup
    libplumbr_free(p);
    return 0;
}
```

### Linking

```bash
# With static library
gcc myapp.c -lplumbr -lpcre2-8 -lpthread -o myapp

# With shared library
gcc myapp.c -L/usr/local/lib -lplumbr -lpcre2-8 -lpthread -o myapp
```

### Configuration

```c
libplumbr_config_t cfg = {
    .pattern_file = "patterns/all.txt",   // Custom pattern file
    .compliance = "hipaa,pci",            // Compliance profiles
    .num_threads = 0,                     // 0 = auto-detect
    .quiet = 1                            // Suppress stats output
};
libplumbr_t *p = libplumbr_new(&cfg);
```

## Architecture

```
input → SSE4.2 Sentinel → Hot AC DFA (L1) → Cold AC DFA → PCRE2 JIT → output
          ↓ fast reject     ↓ top-20          ↓ all literals   ↓ regex verify
        ~90% skipped      ~5% match         ~5% match       ~1-2% match
```

| Optimization | Detail |
|---|---|
| Two-tier AC DFA | Hot (20 patterns) fits in L1 cache, cold DFA handles all patterns |
| Bitmap-compressed DFA | Sparse rows use 256-bit bitmap + popcount for cache efficiency |
| SSE4.2 sentinel | `PCMPISTRI` scans 16 bytes/cycle for trigger characters |
| Arena allocation | Zero `malloc` on hot path — all scratch memory pre-allocated |
| PCRE2 JIT | Native-code regex with JIT for both literal and no-literal patterns |
| Pattern type enum | Integer `switch` replaces `strcmp` for hot-path dispatch |
| Fused I/O | Single `memcpy` for line + newline, 256KB read / 128KB write buffers |
| ReDoS protection | Match limits prevent regex catastrophic backtracking attacks |
| Auto-tuned threads | CPU-aware thread count (1.75× physical cores on Zen 3/4) |

## Benchmark

| Scenario | Throughput | MB/s | Config |
|---|---|---|---|
| 1M clean lines | **9.21M lines/sec** | 861 | auto (14 threads) |
| 5M enterprise (5% secrets) | **8.77M lines/sec** | 821 | auto (14 threads) |
| 1M 10% secrets | 8.24M lines/sec | 772 | auto (14 threads) |
| 1M 100% secrets | 5.52M lines/sec | 528 | auto (14 threads) |
| 1M clean lines (1T) | 3.25M lines/sec | 303 | 1 thread |
| 5M enterprise (1T) | 3.12M lines/sec | 292 | 1 thread |

Benchmarked on AMD Zen 3 (Ryzen 5000, 8C/16T). Results may vary by ±5% between runs.

## Pattern Library

14 built-in patterns covering common secret types:

**Credentials**: AWS access keys, passwords, generic secrets • **Tokens**: GitHub (`ghp_`), JWT (`eyJ`), API keys • **Keys**: RSA/EC private keys • **PII**: Email addresses, SSN, credit cards, IPv4 • **Infrastructure**: Database connection URIs

### Compliance Profiles

| Profile | Patterns | Coverage |
|---|---|---|
| `hipaa` | 27 | MRN, NPI, ICD-10, DEA numbers, insurance IDs |
| `pci` | 16 | Track data, PIN blocks, PAN, CVV, mag stripe |
| `gdpr` | 23 | IBAN, EU national IDs, VAT numbers |
| `soc2` | 25 | Audit logs, access tokens, session IDs |

## Build Targets

```bash
make            # Static library (libplumbr.a)
make shared     # Shared library (libplumbr.so)
make libs       # Both static and shared
make gpu        # Build with OpenCL GPU acceleration
make test       # Unit tests (45 tests across 5 suites)
make gpu-test   # Unit tests with GPU enabled
make sanitize   # Build with ASan + UBSan
make benchmark-full   # Performance benchmarks (CPU)
make gpu-benchmark    # Performance benchmarks (GPU)
make fuzz       # Build libFuzzer harnesses
make install    # Install to /usr/local
make clean      # Remove build artifacts
```

## GPU Acceleration

Optional OpenCL GPU acceleration for Aho-Corasick DFA scanning. Works with **AMD, NVIDIA, and Intel GPUs** — any device that supports OpenCL 1.2+.

```bash
# Install OpenCL development headers
sudo apt install ocl-icd-opencl-dev opencl-headers

# GPU driver (pick one):
sudo apt install mesa-opencl-icd      # AMD (open-source)
sudo apt install nvidia-opencl-icd     # NVIDIA
sudo apt install intel-opencl-icd      # Intel

# Build with GPU support
make gpu

# Benchmark GPU vs CPU
make gpu-benchmark

# Disable GPU at runtime (force CPU-only)
PLUMBR_NO_GPU=1 ./your_app
```

| Feature | Detail |
|---|---|
| Kernel | AC DFA scan — one GPU thread per line |
| Fallback | Automatic CPU fallback if no GPU detected |
| Zero-copy | Uses `CL_MEM_ALLOC_HOST_PTR` for APU shared memory |
| PCRE2 | Stays on CPU (regex backtracking is CPU-suited) |
| Build flag | `-DPLUMBR_GPU` — zero overhead when disabled |

## License

Source Available — free for non-commercial use. Commercial license required for business use. See [LICENSE](LICENSE).

## Contributing

See [CONTRIBUTING.md](.github/CONTRIBUTING.md). Run `make sanitize && make test` before submitting PRs.

---

<p align="center"><strong>PlumbrC</strong> — High-performance log redaction for C applications.</p>
