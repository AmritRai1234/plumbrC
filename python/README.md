# PlumbrC for Python

High-performance log redaction for Python. Python wrapper for the [PlumbrC](https://plumbr.ca) C library.

## Features

- 🚀 **Blazing Fast** — 500K+ lines/sec powered by C engine
- 📦 **702 Built-in Patterns** — AWS, GCP, Stripe, GitHub, and more
- 🔒 **Security-First** — Detect and redact secrets, API keys, passwords, PII
- 💻 **Simple API** — Pythonic interface to high-performance C library
- 🔧 **Zero Dependencies** — Pure ctypes wrapper, no external Python packages

## Installation

```bash
pip install plumbrc
```

### Requirements

- Python 3.7+
- Linux or macOS
- libpcre2 (usually pre-installed)

## Quick Start

```python
from plumbrc import Plumbr

# Create redactor with default patterns
p = Plumbr()

# Redact secrets from text
text = "api_key=sk-proj-abc123 password=secret123"
safe = p.redact(text)
print(safe)
# Output: "api_key=[REDACTED:api_key] password=[REDACTED:password]"

# Check loaded patterns
print(f"Loaded {p.pattern_count} patterns")

# Get statistics
print(p.stats)
```

## Usage

### Basic Redaction

```python
from plumbrc import Plumbr

p = Plumbr()

# Single line
safe = p.redact("AWS key: AKIAIOSFODNN7EXAMPLE")
# "AWS key: [REDACTED:aws_access_key]"

# Multiple lines
lines = [
    "User login with token: ghp_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
    "Database: postgres://user:pass@localhost:5432/db",
    "Normal log line"
]
redacted = p.redact_lines(lines)
```

### Context Manager

```python
with Plumbr() as p:
    safe = p.redact("secret data")
    print(safe)
# Resources automatically cleaned up
```

### Custom Patterns

```python
# Load custom pattern file
p = Plumbr(pattern_file="my_patterns.txt")

# Load patterns from directory
p = Plumbr(pattern_dir="patterns/")
```

### Statistics

```python
p = Plumbr()
p.redact("api_key=secret123")

stats = p.stats
print(stats)
# {
#     'lines_processed': 1,
#     'lines_modified': 1,
#     'patterns_matched': 1,
#     'bytes_processed': 17,
#     'elapsed_seconds': 0.000123
# }
```

## API Reference

### `Plumbr`

Main redaction class.

#### Constructor

```python
Plumbr(
    pattern_file: Optional[str] = None,
    pattern_dir: Optional[str] = None,
    num_threads: int = 0,
    quiet: bool = True
)
```

**Parameters:**
- `pattern_file`: Path to custom pattern file
- `pattern_dir`: Path to directory with pattern files
- `num_threads`: Number of worker threads (0 = auto)
- `quiet`: Suppress statistics output

#### Methods

##### `redact(text: str) -> str`

Redact secrets from text.

```python
safe = p.redact("password=secret123")
```

##### `redact_lines(lines: List[str]) -> List[str]`

Redact multiple lines.

```python
safe_lines = p.redact_lines(["line1", "line2"])
```

#### Properties

##### `pattern_count: int`

Number of loaded patterns.

```python
count = p.pattern_count
```

##### `stats: Dict[str, int]`

Processing statistics.

```python
stats = p.stats
```

##### `version() -> str` (static)

Get PlumbrC library version.

```python
ver = Plumbr.version()
```

## Pattern Categories

PlumbrC includes 702 patterns across 12 categories:

| Category | Patterns | Examples |
|----------|----------|----------|
| Cloud | 109 | AWS, GCP, Azure, DigitalOcean |
| Communication | 127 | Slack, Discord, Telegram, Twilio |
| Payment | 79 | Stripe, Square, PayPal, Braintree |
| VCS | 63 | GitHub, GitLab, Bitbucket |
| Infrastructure | 72 | Docker, NPM, PyPI, CI/CD |
| Crypto | 54 | SSH, SSL/TLS, PGP, JWT |
| Secrets | 52 | Generic API keys, passwords |
| Social | 50 | Facebook, Twitter, LinkedIn |
| Database | 42 | PostgreSQL, MongoDB, MySQL |
| Analytics | 30 | Google Analytics, Mixpanel |
| PII | 14 | Email, SSN, phone, IP |
| Auth | 10 | JWT, OAuth, Bearer tokens |

## Performance

PlumbrC is built for speed:

- **500K+ lines/sec** on modern hardware
- **Sub-microsecond** per-line latency
- **Zero-allocation** hot path in C engine
- **Parallel processing** support

```python
import time
from plumbrc import Plumbr

p = Plumbr()

# Benchmark
lines = ["test line"] * 100000
start = time.time()
p.redact_lines(lines)
elapsed = time.time() - start

print(f"Processed {len(lines)} lines in {elapsed:.2f}s")
print(f"Throughput: {len(lines)/elapsed:.0f} lines/sec")
```

## Examples

### Log File Processing

```python
from plumbrc import Plumbr

with Plumbr() as p:
    with open("app.log") as f:
        for line in f:
            safe = p.redact(line)
            print(safe, end="")
```

### Flask Integration

```python
from flask import Flask, request, jsonify
from plumbrc import Plumbr

app = Flask(__name__)
redactor = Plumbr()

@app.route("/api/redact", methods=["POST"])
def redact():
    text = request.json.get("text", "")
    safe = redactor.redact(text)
    return jsonify({"redacted": safe, "stats": redactor.stats})
```

### Batch Processing

```python
from plumbrc import Plumbr
import glob

p = Plumbr()

for log_file in glob.glob("logs/*.log"):
    with open(log_file) as f:
        lines = f.readlines()
    
    redacted = p.redact_lines(lines)
    
    with open(f"redacted/{log_file}", "w") as f:
        f.writelines(redacted)
```

## Development

### Building from Source

```bash
# Clone repository
git clone https://github.com/AmritRai1234/plumbrC.git
cd plumbrC/python

# Install in development mode
pip install -e ".[dev]"

# Run tests
pytest tests/ -v
```

### Running Tests

```bash
# Install test dependencies
pip install pytest pytest-cov

# Run tests
pytest tests/

# With coverage
pytest tests/ --cov=plumbrc --cov-report=html
```

## License

Source Available License — Free for non-commercial use. See [LICENSE](../LICENSE) for details.

## Links

- **Homepage**: https://plumbr.ca
- **GitHub**: https://github.com/AmritRai1234/plumbrC
- **PyPI**: https://pypi.org/project/plumbrc/
- **Documentation**: https://plumbr.ca

## Support

- **Issues**: [GitHub Issues](https://github.com/AmritRai1234/plumbrC/issues)
- **Commercial Licensing**: See LICENSE file

---

**PlumbrC** — Protect your logs. Protect your secrets.
