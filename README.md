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

PlumbrC detects and removes secrets from log streams in real time. Two-phase matching -- Aho-Corasick literal scan + PCRE2 JIT regex -- achieves 2.6M lines/sec on commodity hardware.

**Website**: [plumbr.ca](https://plumbr.ca) | **Developer Portal**: [plumbr.ca/developers](https://plumbr.ca/developers) | **Docs**: [plumbr.ca/playground](https://plumbr.ca/playground)

## Quick Start

```bash
make
./build/bin/plumbr < app.log > redacted.log
```

### Install

```bash
# Debian/Ubuntu
sudo apt install build-essential libpcre2-dev
make && sudo make install

# Python
pip install plumbrc

# Docker
docker build -t plumbrc . && docker run -i plumbrc < input.log > output.log
```

## REST API

```bash
curl -X POST https://plumbr.ca/api/redact \
  -H "Authorization: Bearer plumbr_live_xxx" \
  -H "Content-Type: application/json" \
  -d '{"text": "key=AKIAIOSFODNN7EXAMPL3"}'
```

```json
{
  "redacted": "key=[REDACTED:aws_access_key]",
  "stats": { "lines_modified": 1, "patterns_matched": 1 }
}
```

Get your API key at [plumbr.ca/developers](https://plumbr.ca/developers).

## Python

```python
from plumbrc import Plumbr

with Plumbr() as p:
    safe = p.redact("password=secret123")
    # "password=[REDACTED:password]"
```

## Pattern Library

702 patterns across 12 categories: Cloud (AWS, GCP, Azure), Communication (Slack, Discord), Payment (Stripe, PayPal), VCS (GitHub, GitLab), Infrastructure, Crypto, Secrets, Social, Database, Analytics, PII, and Auth.

```bash
./plumbr -p patterns/all.txt < app.log > redacted.log
./plumbr -p patterns/cloud/aws.txt < app.log > redacted.log
```

## Architecture

```
stdin -> Aho-Corasick (O(n) literal scan) -> PCRE2 JIT (regex verify ~1-5% of lines) -> stdout
```

| Optimization | Detail |
|---|---|
| Arena allocation | Zero malloc in hot path |
| PCRE2 JIT | Native code regex |
| 64KB aligned buffers | Cache-friendly I/O |
| ReDoS protection | Match limits on backtracking |

## License

Source Available -- free for non-commercial use. Commercial license required for business use. See [LICENSE](LICENSE).

## Contributing

See [CONTRIBUTING.md](.github/CONTRIBUTING.md). Run `make sanitize && make test` before submitting PRs.

---

<p align="center"><strong>PlumbrC</strong> -- Protect your logs. Protect your secrets.</p>
