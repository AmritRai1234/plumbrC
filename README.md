<p align="center">
  <h1 align="center">PlumbrC</h1>
  <p align="center"><strong>High-Performance Log Redaction Engine</strong></p>
  <p align="center">Pure C11 &bull; 5M+ lines/sec &bull; Zero-allocation hot path &bull; 793 patterns</p>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/language-C11-blue.svg" alt="Language">
  <img src="https://img.shields.io/badge/performance-5M%20lines%2Fsec-brightgreen.svg" alt="Performance">
  <img src="https://img.shields.io/badge/patterns-793-green.svg" alt="Patterns">
  <img src="https://img.shields.io/badge/memory-0%20leaks-success.svg" alt="Memory">
  <img src="https://img.shields.io/badge/python-pip%20install%20plumbrc-3776AB.svg?logo=python&logoColor=white" alt="Python">
  <img src="https://img.shields.io/badge/license-Source%20Available-orange.svg" alt="License">
</p>

---

PlumbrC detects and removes secrets from log streams in real time. Three-phase matching — SSE4.2 sentinel scan → Aho-Corasick literal DFA → PCRE2 JIT regex — achieves **5M+ lines/sec** on commodity hardware with 793 patterns.

**Website**: [plumbr.ca](https://plumbr.ca) | **Developer Portal**: [plumbr.ca/developers](https://plumbr.ca/developers) | **Docs**: [plumbr.ca/playground](https://plumbr.ca/playground)

## Performance

| Scenario | Throughput | MB/s | Config |
|---|---|---|---|
| 5M enterprise (5% secrets) | **5.15M lines/sec** | 482 | 8 threads, 702 patterns |
| 1M clean lines | 3.70M lines/sec | 346 | 1 thread |
| 1M 10% secrets | 3.16M lines/sec | 296 | 1 thread |
| Python bulk API (1K batch) | 1.78M lines/sec | — | ctypes, 702 patterns |
| Python single-line | 401K lines/sec | — | ctypes, 702 patterns |

Benchmarked on AMD Ryzen 5 5600X (6C/12T), 32GB DDR4-3200.

```bash
make benchmark-full   # Human-readable results
make benchmark-json   # JSON for CI regression checks
```

## Quick Start

```bash
# Build
sudo apt install build-essential libpcre2-dev
make && sudo make install

# Redact stdin → stdout
./build/bin/plumbr < app.log > redacted.log

# Custom patterns
./build/bin/plumbr -p patterns/all.txt < app.log > redacted.log

# HIPAA + PCI compliance
./build/bin/plumbr -c hipaa,pci < patient.log > redacted.log

# Pipeline
tail -f /var/log/app.log | plumbr | tee redacted.log
```

## Architecture

```
stdin → SSE4.2 Sentinel → Hot AC DFA (L1 cache) → Cold AC DFA → PCRE2 JIT → stdout
          ↓ fast reject     ↓ top-20 literals       ↓ all literals    ↓ regex verify
        ~90% skipped      ~5% match               ~5% match        ~1-2% match
```

| Optimization | Detail |
|---|---|
| Two-level AC DFA | Hot (20 patterns) fits in L1 cache, cold handles all 702 |
| SSE4.2 sentinel | PCMPISTRI scans 16 bytes/cycle for trigger characters |
| Arena allocation | Zero malloc in hot path |
| PCRE2 JIT | Native-code regex for final verification |
| PGO build | Profile-guided optimization via `make pgo` |
| ReDoS protection | Match limits prevent regex backtracking attacks |

## Compliance Profiles

| Profile | Patterns | Coverage |
|---|---|---|
| `hipaa` | 27 | MRN, NPI, ICD-10, DEA numbers, insurance IDs |
| `pci` | 16 | Track data, PIN blocks, PAN, CVV, mag stripe |
| `gdpr` | 23 | IBAN, EU national IDs, VAT numbers |
| `soc2` | 25 | Audit logs, access tokens, session IDs |
| `all` | 91 | All compliance profiles combined |

```bash
# CLI
plumbr -c hipaa,pci < logs.txt > safe.txt
plumbr -c all < logs.txt > safe.txt
```

## Pattern Library

702 base patterns + 91 compliance patterns across 12 categories:

**Cloud**: AWS, GCP, Azure &bull; **Communication**: Slack, Discord, Teams &bull; **Payment**: Stripe, PayPal &bull; **VCS**: GitHub, GitLab, Bitbucket &bull; **Infrastructure**: SSH, TLS, Docker &bull; **Crypto**: Private keys, mnemonics &bull; **Auth**: JWT, OAuth, API keys &bull; **PII**: SSN, email, phone &bull; **Database**: connection strings &bull; **Analytics**: Mixpanel, Segment &bull; **Social**: Facebook, Twitter &bull; **Secrets**: Generic passwords, tokens

## C Library API

```c
#include <plumbr/libplumbr.h>

// Create with default patterns
libplumbr_t *p = libplumbr_new(NULL);

// Create with custom config
libplumbr_config_t cfg = {
    .pattern_file = "patterns/all.txt",
    .compliance = "hipaa,pci",
    .num_threads = 0,  // auto-detect
    .quiet = 1
};
libplumbr_t *p = libplumbr_new(&cfg);

// Redact a single line
char *safe = libplumbr_redact(p, "password=secret123", 18, NULL);
printf("%s\n", safe);  // "password=[REDACTED:password]"
libplumbr_free_string(safe);

// Bulk redact (best for FFI bindings)
size_t out_len;
char *result = libplumbr_redact_buffer(p, input_buf, input_len, &out_len);
libplumbr_free_string(result);

// Cleanup
libplumbr_free(p);
```

Build: `make lib` (static) or `make shared` (dynamic).

## Python

```bash
pip install plumbrc
```

```python
from plumbrc import Plumbr

# Default patterns
with Plumbr() as p:
    safe = p.redact("password=secret123")
    # "password=[REDACTED:password]"

# Compliance patterns
with Plumbr(compliance=['hipaa', 'pci']) as p:
    safe = p.redact("MRN: ABC123456")
    # "MRN: [REDACTED:mrn]"

# Bulk API (4x faster than single-line)
with Plumbr(pattern_file="/path/to/all.txt") as p:
    results = p.redact_lines(["line1", "line2", ...])
    # Or: result_text = p.redact_bulk("line1\nline2\n...")
```

## HTTP Server

```bash
make server
./build/bin/plumbr-server --port 8080 --threads 4 --pattern-file patterns/all.txt

# Health check
curl http://localhost:8080/health

# Redact
curl -X POST http://localhost:8080/api/redact \
  -H "Content-Type: application/json" \
  -d '{"text": "key=AKIAIOSFODNN7EXAMPLE"}'
```

## REST API (Cloud)

```bash
curl -X POST https://plumbr.ca/api/redact \
  -H "Authorization: Bearer plumbr_live_xxx" \
  -H "Content-Type: application/json" \
  -d '{"text": "key=AKIAIOSFODNN7EXAMPL3"}'
```

Get your API key at [plumbr.ca/developers](https://plumbr.ca/developers).

## Deployment

### Docker

```bash
docker build -t plumbrc .
echo "password=secret" | docker run -i plumbrc
```

### Kubernetes (DaemonSet)

```bash
kubectl apply -f integrations/kubernetes/daemonset.yaml
```

### Kubernetes (Sidecar)

```bash
kubectl apply -f integrations/kubernetes/sidecar.yaml
```

### systemd

```bash
sudo cp integrations/systemd/plumbr-server.service /etc/systemd/system/
sudo systemctl enable --now plumbr-server
```

### Log Pipelines

- **Fluentd**: `integrations/fluentd/fluent.conf`
- **Logstash**: `integrations/logstash/plumbr.conf`
- **Vector**: `integrations/vector/vector.toml`

## Build Targets

```bash
make                # CLI binary (release)
make debug          # Debug build with sanitizers
make lib            # Static library (libplumbr.a)
make shared         # Shared library (libplumbr.so)
make server         # HTTP server
make pgo            # Profile-guided optimized build
make test-unit      # Unit tests
make benchmark-full # Performance benchmarks
make valgrind       # Memory leak check
make install        # Install to /usr/local
```

## License

Source Available — free for non-commercial use. Commercial license required for business use. See [LICENSE](LICENSE).

## Contributing

See [CONTRIBUTING.md](.github/CONTRIBUTING.md). Run `make sanitize && make test-unit` before submitting PRs.

---

<p align="center"><strong>PlumbrC</strong> — Protect your logs. Protect your secrets.</p>
