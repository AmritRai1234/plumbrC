<p align="center">
  <h1 align="center">🔧 PlumbrC</h1>
  <p align="center"><strong>High-Performance Log Redaction Pipeline</strong></p>
  <p align="center">Pure C implementation • 500K+ lines/sec • Zero-allocation hot path</p>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/language-C11-blue.svg" alt="Language">
  <img src="https://img.shields.io/badge/patterns-702-green.svg" alt="Patterns">
  <img src="https://img.shields.io/badge/license-Source%20Available-orange.svg" alt="License">
</p>

---

## ✨ Features

- **🚀 Blazing Fast** — Two-phase matching with Aho-Corasick literal scan + PCRE2 regex verification
- **💾 Zero-Allocation Hot Path** — Arena-based memory with no malloc in processing loop  
- **🔗 Pipeline-Friendly** — Designed for Unix stdin/stdout chaining
- **📦 702 Built-in Patterns** — Comprehensive secret detection (AWS, GCP, Stripe, GitHub, etc.)
- **🔒 Security-First** — Bounds checking, input validation, ReDoS protection, sanitizer-ready
- **⚡ Parallel Processing** — Multi-threaded support for maximum throughput
- **📚 Library API** — Embed redaction directly in your applications

## 🚀 Quick Start

```bash
# Build
make

# Run
./build/bin/plumbr < app.log > redacted.log

# Pipeline usage
tail -f /var/log/app.log | ./build/bin/plumbr | tee redacted.log
```

## 📦 Installation

### From Source

```bash
# Install dependencies (Debian/Ubuntu)
sudo apt install build-essential libpcre2-dev

# Build optimized release
make

# Install to /usr/local/bin
sudo make install
```

### Docker

```bash
# Build image
docker build -t plumbrc -f integrations/docker/Dockerfile .

# Run
docker run -i plumbrc < input.log > output.log
```

## 🛠️ Build Options

| Command | Description |
|---------|-------------|
| `make` | Optimized release build (-O3, LTO) |
| `make debug` | Debug build with symbols |
| `make sanitize` | Build with AddressSanitizer/UBSan |
| `make profile` | Build for gprof profiling |
| `make lib` | Build static library (libplumbr.a) |
| `make shared` | Build shared library (libplumbr.so) |
| `make benchmark` | Run performance benchmark |
| `make test` | Run basic functionality test |

## 📖 Usage

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

# Load AWS-specific patterns
./plumbr -p patterns/cloud/aws.txt < app.log > redacted.log

# Combine defaults with extra patterns
./plumbr -d -p patterns/social/social.txt < app.log > redacted.log

# Use all 702 patterns
./plumbr -p patterns/all.txt < app.log > redacted.log

# Real-time pipeline
tail -f /var/log/app.log | ./plumbr | tee redacted.log
```

## 📋 Pattern Format

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

## 📁 Pattern Library

PlumbrC includes **702 patterns** organized by category:

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

```bash
# Load all patterns
./plumbr -p patterns/all.txt

# Load specific category
./plumbr -p patterns/cloud/aws.txt
./plumbr -p patterns/payment/stripe.txt
```

## ⚙️ Architecture

```
stdin → [Read Buffer 64KB] → [Line Parser] → [Aho-Corasick] → [PCRE2 Verify] → [Redact] → [Write Buffer 64KB] → stdout
```

### Two-Phase Matching

1. **Phase 1: Aho-Corasick** — O(n) scan for all pattern literals simultaneously
2. **Phase 2: PCRE2 JIT** — Regex verification only on lines with literal matches (~1-5% of traffic)

### Performance Optimizations

| Optimization | Description |
|--------------|-------------|
| Aho-Corasick first pass | O(n) multi-pattern matching |
| PCRE2 JIT compilation | Native code regex execution |
| Arena allocation | Single mmap, no malloc/free during processing |
| 64KB L2-aligned buffers | Cache-friendly I/O |
| Vectorized writes | writev() for batch output |
| ReDoS protection | Match limits prevent catastrophic backtracking |

## 📚 Library API

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

### API Functions

| Function | Description |
|----------|-------------|
| `libplumbr_new()` | Create PlumbrC instance |
| `libplumbr_redact()` | Redact a single line (returns allocated string) |
| `libplumbr_redact_inplace()` | Redact in-place if buffer is large enough |
| `libplumbr_redact_batch()` | Batch process multiple lines |
| `libplumbr_get_stats()` | Get processing statistics |
| `libplumbr_pattern_count()` | Get number of loaded patterns |
| `libplumbr_free()` | Free instance |

### Building with Library

```bash
# Build static library
make lib

# Compile with static library
gcc -I./include example.c -L./build/lib -lplumbr -lpcre2-8 -o example

# Build shared library
make shared

# Compile with shared library
gcc -I./include example.c -L./build/lib -lplumbr -lpcre2-8 -Wl,-rpath,./build/lib -o example
```

## 🔌 Integrations

Ready-to-use configurations for popular log pipelines:

| Platform | Location | Description |
|----------|----------|-------------|
| Docker | `integrations/docker/` | Dockerfile, docker-compose examples |
| Kubernetes | `integrations/kubernetes/` | Sidecar, DaemonSet, init container configs |
| Fluentd | `integrations/fluentd/` | Filter plugin configuration |
| Logstash | `integrations/logstash/` | Pipeline filter configuration |
| Vector | `integrations/vector/` | Transform configuration |
| systemd | `integrations/systemd/` | Service unit files |

### Usage Patterns

```bash
# 1. Pipe Mode
your-app | plumbr | destination

# 2. File Mode
plumbr < input.log > output.log

# 3. Sidecar Mode (Kubernetes)
# See integrations/kubernetes/

# 4. DaemonSet Mode (Kubernetes)
# See integrations/kubernetes/
```

## 🧪 Testing

```bash
# Quick functionality test
make test

# Build and run unit tests
make test-unit

# Run benchmark
make benchmark

# Run with sanitizers
make sanitize
./build/bin/plumbr < test_data/sample.log > /dev/null
```

## 📊 Performance

Targeting **500K+ lines/second** on modern hardware.

```bash
# Generate test data
make test-data

# Run benchmark
make benchmark
```

### Benchmark Results

Test on representative data shows:
- **High throughput**: ~500K lines/sec single-threaded
- **Low latency**: Sub-microsecond per line
- **Linear scaling**: Near-linear with parallel threads

## 🏗️ Project Structure

```
plumbrC/
├── src/                    # Source files
│   ├── main.c              # CLI entry point
│   ├── pipeline.c          # Processing pipeline
│   ├── aho_corasick.c      # Multi-pattern matching
│   ├── redactor.c          # Two-phase redaction engine
│   ├── patterns.c          # Pattern management
│   ├── arena.c             # Arena memory allocator
│   ├── io.c                # Buffered I/O
│   ├── parallel.c          # Parallel processing
│   ├── thread_pool.c       # Thread pool implementation
│   ├── hwdetect.c          # Hardware detection
│   └── libplumbr.c         # Library API
├── include/                # Header files
│   ├── plumbr.h            # Public CLI API
│   ├── libplumbr.h         # Library API
│   ├── config.h            # Configuration constants
│   └── ...                 # Internal headers
├── patterns/               # 702 secret detection patterns
│   ├── all.txt             # All patterns combined
│   ├── default.txt         # Default 42 patterns
│   ├── cloud/              # AWS, GCP, Azure patterns
│   ├── payment/            # Stripe, PayPal patterns
│   └── ...                 # More categories
├── integrations/           # Deployment configs
│   ├── docker/
│   ├── kubernetes/
│   ├── fluentd/
│   └── ...
├── tests/                  # Test files
│   ├── test_patterns.c
│   ├── test_redactor.c
│   └── benchmark.c
├── examples/               # Usage examples
│   ├── c/example.c
│   └── python/example.py
├── Makefile
└── README.md
```

## 🔧 Dependencies

- **GCC or Clang** — C11 compiler
- **PCRE2** — Perl Compatible Regular Expressions

```bash
# Debian/Ubuntu
sudo apt install build-essential libpcre2-dev

# macOS
brew install pcre2

# RHEL/CentOS/Fedora
sudo dnf install gcc pcre2-devel
```

## 📄 License

**Source Available License** — Free for non-commercial use, commercial license required for business use.

### Free for:
- Personal projects
- Educational purposes
- Academic research
- Non-profit organizations
- Open source projects (non-commercial)

### Commercial Use:
Requires a paid license. See [LICENSE](LICENSE) for details and contact information.

## 🤝 Contributing

Contributions are welcome! By contributing, you agree that your contributions will be licensed under the same Source Available License.

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Run tests: `make sanitize && make test`
5. Submit a pull request

## 📞 Support

- **Issues**: Open a GitHub issue for bugs or feature requests
- **Commercial Licensing**: See [LICENSE](LICENSE) for contact information
- **Documentation**: See `patterns/README.md` for pattern library documentation

---

<p align="center">
  <strong>PlumbrC</strong> — Protect your logs. Protect your secrets.
</p>
