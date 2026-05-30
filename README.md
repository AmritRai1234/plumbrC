<p align="center">
  <h1 align="center">PlumbrC</h1>
  <p align="center"><strong>High-Performance Log Redaction Library</strong></p>
  <p align="center">Pure C11 &bull; 6.4M lines/sec &bull; Zero-allocation hot path &bull; 793 patterns</p>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/language-C11-blue.svg" alt="Language">
  <img src="https://img.shields.io/badge/performance-6.4M%20lines%2Fsec-brightgreen.svg" alt="Performance">
  <img src="https://img.shields.io/badge/patterns-793-green.svg" alt="Patterns">
  <img src="https://img.shields.io/badge/memory-0%20leaks-success.svg" alt="Memory">
  <img src="https://img.shields.io/badge/license-Source%20Available-orange.svg" alt="License">
</p>

---

PlumbrC is a C11 library that detects and removes secrets from text at high speed. Three-phase matching — SSE4.2 sentinel scan → Aho-Corasick literal DFA → PCRE2 JIT regex — achieves **6.4M lines/sec** on commodity hardware with 793 patterns.

## Build

```bash
# Dependencies
sudo apt install build-essential libpcre2-dev

# Build static library (libplumbr.a)
make

# Build shared library (libplumbr.so)
make shared

# Build both
make libs

# Run tests
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
| Two-level AC DFA | Hot (20 patterns) fits in L1 cache, cold handles all 702 |
| SSE4.2 sentinel | PCMPISTRI scans 16 bytes/cycle for trigger characters |
| Arena allocation | Zero malloc in hot path |
| PCRE2 JIT | Native-code regex for final verification |
| No-Literal bypass | Fast-path pre-checks avoid PCRE2 on clean lines |
| ReDoS protection | Match limits prevent regex backtracking attacks |

## Benchmark

| Scenario | Throughput | MB/s | Config |
|---|---|---|---|
| 5M enterprise (5% secrets) | **6.37M lines/sec** | 596 | auto (10 threads) |
| 1M clean lines | 5.89M lines/sec | 550 | auto (10 threads) |
| 1M 10% secrets | 6.25M lines/sec | 585 | auto (10 threads) |
| 1M 100% secrets | 4.09M lines/sec | 392 | auto (10 threads) |
| 1M clean lines (1T) | 2.79M lines/sec | 260 | 1 thread |
| 5M enterprise (1T) | 2.67M lines/sec | 250 | 1 thread |

Benchmarked on AMD Zen 3 (8C/16T).

## Pattern Library

793 patterns across 12 categories:

**Cloud**: AWS, GCP, Azure • **Communication**: Slack, Discord, Teams • **Payment**: Stripe, PayPal • **VCS**: GitHub, GitLab, Bitbucket • **Infrastructure**: SSH, TLS, Docker • **Crypto**: Private keys, mnemonics • **Auth**: JWT, OAuth, API keys • **PII**: SSN, email, phone • **Database**: connection strings • **Analytics**: Mixpanel, Segment • **Social**: Facebook, Twitter • **Secrets**: Generic passwords, tokens

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
make test       # Unit tests
make sanitize   # Build with ASan + UBSan
make benchmark-full  # Performance benchmarks
make fuzz       # Build libFuzzer harnesses
make install    # Install to /usr/local
make clean      # Remove build artifacts
```

## License

Source Available — free for non-commercial use. Commercial license required for business use. See [LICENSE](LICENSE).

## Contributing

See [CONTRIBUTING.md](.github/CONTRIBUTING.md). Run `make sanitize && make test` before submitting PRs.

---

<p align="center"><strong>PlumbrC</strong> — High-performance log redaction for C applications.</p>
