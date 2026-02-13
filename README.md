<p align="center">
  <h1 align="center">PlumbrC</h1>
  <p align="center"><strong>High-Performance Log Redaction Engine</strong></p>
  <p align="center">Pure C11 &bull; 2.6M lines/sec &bull; Zero-allocation hot path &bull; 702 patterns</p>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/language-C11-blue.svg" alt="Language">
  <img src="https://img.shields.io/badge/patterns-702-green.svg" alt="Patterns">
  <img src="https://img.shields.io/badge/license-Source%20Available-orange.svg" alt="License">
  <img src="https://img.shields.io/badge/python-pip%20install%20plumbrc-3776AB.svg?logo=python&logoColor=white" alt="Python">
  <a href="https://github.com/AmritRai1234/plumbrC/discussions"><img src="https://img.shields.io/badge/discussions-join-blue.svg?logo=github" alt="Discussions"></a>
</p>

---

PlumbrC is a high-performance log redaction engine written in C11 that detects and removes secrets from log streams in real time. It uses a two-phase matching pipeline — Aho-Corasick literal scan followed by PCRE2 JIT regex verification — to achieve throughput exceeding 2.6 million lines per second on commodity hardware.

**Website**: [plumbr.ca](https://plumbr.ca) | **Developer Portal**: [plumbr.ca/developers](https://plumbr.ca/developers) | **Playground**: [plumbr.ca/playground](https://plumbr.ca/playground)

---

## Table of Contents

- [Quick Start](#quick-start)
- [Installation](#installation)
- [Usage](#usage)
- [Pattern Library](#pattern-library)
- [Architecture](#architecture)
- [Library API](#library-api)
- [Python Package](#python-package)
- [REST API](#rest-api)
- [Web Application](#web-application)
- [Developer Portal](#developer-portal)
- [Testing](#testing)
- [Performance](#performance)
- [Project Structure](#project-structure)
- [Dependencies](#dependencies)
- [License](#license)
- [Contributing](#contributing)

---

## Quick Start

```bash
# Build
make

# Redact a log file
./build/bin/plumbr < app.log > redacted.log

# Real-time pipeline
tail -f /var/log/app.log | ./build/bin/plumbr | tee redacted.log
```

## Installation

### From Source

```bash
# Install dependencies (Debian/Ubuntu)
sudo apt install build-essential libpcre2-dev

# Build optimized release
make

# Install system-wide
sudo make install
```

### Python

```bash
pip install plumbrc
```

### Docker

```bash
docker build -t plumbrc -f integrations/docker/Dockerfile .
docker run -i plumbrc < input.log > output.log
```

## Usage

```
plumbr [OPTIONS] < input > output

Options:
  -p, --patterns FILE   Load patterns from FILE
  -d, --defaults        Use built-in default patterns (default: on)
  -D, --no-defaults     Disable built-in default patterns
  -j, --threads N       Use N worker threads (0=auto)
  -q, --quiet           Suppress statistics output
  -s, --stats           Print statistics to stderr
  -H, --hwinfo          Show hardware detection info
  -h, --help            Show help
  -v, --version         Show version
```

### Examples

```bash
# Redact using default patterns
./plumbr < app.log > redacted.log

# Use custom patterns
./plumbr -p custom.txt < app.log > redacted.log

# Load category-specific patterns
./plumbr -p patterns/cloud/aws.txt < app.log > redacted.log

# Combine defaults with extra patterns
./plumbr -d -p patterns/social/social.txt < app.log > redacted.log

# Use all 702 patterns
./plumbr -p patterns/all.txt < app.log > redacted.log

# Real-time pipeline
tail -f /var/log/app.log | ./plumbr | tee redacted.log
```

## Build Options

| Command | Description |
|---------|-------------|
| `make` | Optimized release build (-O3, LTO) |
| `make debug` | Debug build with symbols |
| `make sanitize` | Build with AddressSanitizer/UBSan |
| `make profile` | Build for gprof profiling |
| `make lib` | Build static library (libplumbr.a) |
| `make shared` | Build shared library (libplumbr.so) |
| `make benchmark` | Run performance benchmark |
| `make test` | Run functionality tests |

## Pattern Library

PlumbrC ships with **702 patterns** organized by category:

| Category | Patterns | Coverage |
|----------|----------|----------|
| Cloud | 109 | AWS, GCP, Azure, DigitalOcean, Heroku, Alibaba, Oracle |
| Communication | 127 | Slack, Discord, Telegram, Twilio, SendGrid, Mailgun |
| Payment | 79 | Stripe, Square, PayPal, Braintree, Adyen, Checkout.com |
| VCS | 63 | GitHub, GitLab, Bitbucket |
| Infrastructure | 72 | Docker, NPM, PyPI, CI/CD pipelines |
| Crypto | 54 | SSH, SSL/TLS, PGP, JWT |
| Secrets | 52 | Generic API keys, passwords, tokens |
| Social | 50 | Facebook, Twitter, LinkedIn, YouTube, TikTok |
| Database | 42 | PostgreSQL, MongoDB, MySQL, Redis |
| Analytics | 30 | Google Analytics, Mixpanel, Amplitude, Segment |
| PII | 14 | Email, SSN, phone numbers, IP addresses |
| Auth | 10 | JWT, OAuth, Bearer tokens |

### Pattern Format

Patterns are defined in text files with one pattern per line:

```
name|literal|regex|replacement
```

| Field | Description |
|-------|-------------|
| `name` | Unique identifier for the pattern |
| `literal` | Fast literal string for Aho-Corasick pre-filtering |
| `regex` | PCRE2 regular expression for precise matching |
| `replacement` | Replacement text (e.g., `[REDACTED:category]`) |

### Example Patterns

```
aws_key|AKIA|AKIA[0-9A-Z]{16}|[REDACTED:aws_key]
password|password|password[=:]\s*\S+|[REDACTED:password]
github_token|ghp_|ghp_[A-Za-z0-9]{36}|[REDACTED:github_token]
```

## Architecture

```
stdin -> [Read Buffer 64KB] -> [Line Parser] -> [Aho-Corasick] -> [PCRE2 Verify] -> [Redact] -> [Write Buffer 64KB] -> stdout
```

### Two-Phase Matching

1. **Phase 1: Aho-Corasick** -- O(n) scan for all pattern literals simultaneously
2. **Phase 2: PCRE2 JIT** -- Regex verification only on lines with literal matches (~1-5% of traffic)

### Performance Optimizations

| Optimization | Description |
|--------------|-------------|
| Aho-Corasick first pass | O(n) multi-pattern matching |
| PCRE2 JIT compilation | Native code regex execution |
| Arena allocation | Single mmap, no malloc/free during processing |
| 64KB L2-aligned buffers | Cache-friendly I/O |
| Vectorized writes | writev() for batch output |
| ReDoS protection | Match limits prevent catastrophic backtracking |

## Library API

Embed log redaction directly into your application:

```c
#include <libplumbr.h>

int main(void) {
    // Create instance with default patterns
    libplumbr_t *p = libplumbr_new(NULL);

    // Redact a line
    size_t out_len;
    char *safe = libplumbr_redact(p, "api_key=secret123", 17, &out_len);
    printf("%s\n", safe);  // "api_key=[REDACTED:api_key]"
    free(safe);

    // Batch processing
    const char *inputs[] = {"line1", "line2"};
    size_t lens[] = {5, 5};
    char *outputs[2];
    size_t out_lens[2];
    libplumbr_redact_batch(p, inputs, lens, outputs, out_lens, 2);

    // Cleanup
    libplumbr_free(p);
    return 0;
}
```

### API Reference

| Function | Description |
|----------|-------------|
| `libplumbr_new()` | Create PlumbrC instance |
| `libplumbr_redact()` | Redact a single line (returns allocated string) |
| `libplumbr_redact_inplace()` | Redact in-place if buffer is large enough |
| `libplumbr_redact_batch()` | Batch process multiple lines |
| `libplumbr_get_stats()` | Get processing statistics |
| `libplumbr_pattern_count()` | Get number of loaded patterns |
| `libplumbr_free()` | Free instance |

### Linking

```bash
# Static library
make lib
gcc -I./include example.c -L./build/lib -lplumbr -lpcre2-8 -o example

# Shared library
make shared
gcc -I./include example.c -L./build/lib -lplumbr -lpcre2-8 -Wl,-rpath,./build/lib -o example
```

## Python Package

Install the Python wrapper from PyPI:

```bash
pip install plumbrc
```

```python
from plumbrc import Plumbr

# Basic usage
p = Plumbr()
result = p.redact("password=secret123")
print(result)  # "password=[REDACTED:password]"

# Context manager
with Plumbr() as p:
    safe = p.redact("key=AKIAIOSFODNN7EXAMPL3")
    print(safe)  # "key=[REDACTED:aws_key]"

# Batch processing
results = p.redact_batch([
    "aws_key=AKIAIOSFODNN7EXAMPL3",
    "password=hunter2",
    "This line is safe"
])

# Statistics
stats = p.stats()
print(f"Lines processed: {stats['lines_processed']}")
print(f"Patterns matched: {stats['patterns_matched']}")
```

### Performance

- **71,000+ lines/sec** throughput
- **13.9 microseconds** latency per line
- Zero external dependencies

## REST API

Authenticate with your API key. Send text, get clean text back.

```
POST /api/redact
```

```bash
curl -X POST https://plumbr.ca/api/redact \
  -H "Authorization: Bearer plumbr_live_xxx" \
  -H "Content-Type: application/json" \
  -d '{"text": "key=AKIAIOSFODNN7EXAMPL3"}'
```

Response:

```json
{
  "redacted": "key=[REDACTED:aws_access_key]",
  "stats": {
    "lines_modified": 1,
    "patterns_matched": 1
  }
}
```

| Detail | Value |
|--------|-------|
| Authentication | API key (Bearer token) |
| Processing | Sub-millisecond (C engine) |
| Storage | Ephemeral -- nothing is saved |
| Rate limit | 1,000 requests/min (default) |

Get your API key at [plumbr.ca/developers](https://plumbr.ca/developers).

## Web Application

PlumbrC includes a full web interface built on Next.js:

- **Landing Page** -- Product overview and live demo
- **Playground** -- Paste logs and see redaction in real time
- **Developer Portal** -- Create accounts, register apps, manage API keys
- **Community** -- Contribution guides and community resources

### Local Development

```bash
cd web
npm install
npm run dev
# Open http://localhost:3000
```

### Tech Stack

| Layer | Technology |
|-------|------------|
| Framework | Next.js 16 (App Router) |
| Language | TypeScript |
| Styling | Tailwind CSS v4 |
| Database | SQLite (better-sqlite3) |
| Auth | JWT (jose) |
| Icons | Lucide React |

## Developer Portal

The developer portal at [plumbr.ca/developers](https://plumbr.ca/developers) provides:

- **Account creation** -- Sign up with email and password
- **App registration** -- Create up to 5 apps per account
- **API key management** -- Generate, view, regenerate, and revoke keys
- **Key format** -- `plumbr_live_xxx` (public) / `plumbr_secret_xxx` (private)

## Testing

```bash
# Functionality tests
make test

# Unit tests
make test-unit

# Benchmark
make benchmark

# Sanitizer checks
make sanitize
./build/bin/plumbr < test_data/sample.log > /dev/null
```

## Performance

Benchmarked at **2.6M lines/sec** on modern hardware (single-threaded).

```bash
make test-data
make benchmark
```

| Metric | Result |
|--------|--------|
| Throughput | 2.6M lines/sec |
| Latency | Sub-microsecond per line |
| Scaling | Near-linear with threads |
| Memory | Zero allocations in hot path |

## Project Structure

```
plumbrC/
  src/                      Source files
    main.c                  CLI entry point
    pipeline.c              Processing pipeline
    aho_corasick.c          Multi-pattern matching
    redactor.c              Two-phase redaction engine
    patterns.c              Pattern management
    arena.c                 Arena memory allocator
    io.c                    Buffered I/O
    parallel.c              Parallel processing
    libplumbr.c             Library API
  include/                  Header files
  web/                      Next.js web application
    app/
      page.tsx              Landing page
      playground/           Interactive playground
      developers/           Developer portal
      community/            Community page
      api/                  REST API endpoints
      lib/                  Database and auth utilities
  python/                   Python package (plumbrc)
    plumbrc/                Package source
    tests/                  Unit and performance tests
    pyproject.toml          Package configuration
  patterns/                 702 secret detection patterns
    all.txt                 All patterns combined
    default.txt             Default 42 patterns
    cloud/                  AWS, GCP, Azure
    payment/                Stripe, PayPal
  integrations/             Deployment configs
    docker/
    kubernetes/
    fluentd/
    logstash/
    vector/
    systemd/
  .github/                  Community health files
    CONTRIBUTING.md
    CODE_OF_CONDUCT.md
    SECURITY.md
    workflows/              CI/CD pipelines
  Makefile
  Dockerfile
  README.md
```

## Dependencies

- **GCC or Clang** -- C11 compiler
- **PCRE2** -- Perl Compatible Regular Expressions

```bash
# Debian/Ubuntu
sudo apt install build-essential libpcre2-dev

# macOS
brew install pcre2

# RHEL/CentOS/Fedora
sudo dnf install gcc pcre2-devel
```

## License

**Source Available License** -- Free for non-commercial use. Commercial license required for business use.

Free for personal projects, educational purposes, academic research, non-profit organizations, and non-commercial open source projects.

Commercial use requires a paid license. See [LICENSE](LICENSE) for details.

## Contributing

Contributions are welcome. Please read [CONTRIBUTING.md](.github/CONTRIBUTING.md) before submitting a pull request.

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Run tests: `make sanitize && make test`
5. Submit a pull request

## Support

- **Issues**: [GitHub Issues](https://github.com/AmritRai1234/plumbrC/issues)
- **Discussions**: [GitHub Discussions](https://github.com/AmritRai1234/plumbrC/discussions)
- **Security**: [Security Policy](.github/SECURITY.md)
- **Commercial Licensing**: See [LICENSE](LICENSE)

---

<p align="center">
  <strong>PlumbrC</strong> -- Protect your logs. Protect your secrets.
</p>
