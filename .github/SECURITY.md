# Security Policy

## Supported Versions

| Version | Supported          |
|---------|--------------------|
| 1.x     | ✅ Active support  |
| < 1.0   | ❌ No longer supported |

## Reporting a Vulnerability

**Do NOT open a public GitHub issue for security vulnerabilities.**

### How to Report

Send an email to **security@plumbr.ca** with:

1. **Description** of the vulnerability
2. **Steps to reproduce** the issue
3. **Impact assessment** (what could an attacker do?)
4. **Affected versions**
5. **Suggested fix** (if you have one)

### Response Timeline

| Action | Timeframe |
|--------|-----------|
| Acknowledgment | Within 48 hours |
| Initial assessment | Within 1 week |
| Fix development | Within 2 weeks |
| Public disclosure | After fix is released |

### What to Expect

- You will receive an acknowledgment within 48 hours
- We will keep you informed of progress
- We will credit you in the security advisory (unless you prefer otherwise)
- We follow responsible disclosure practices

## Security Best Practices

When using PlumbrC:

- **Keep updated** — Always use the latest version
- **Pattern files** — Verify pattern files come from trusted sources
- **Memory** — PlumbrC uses arena allocation with automatic cleanup
- **Input validation** — The library handles malformed input gracefully
- **ReDoS protection** — All regex patterns have match limits

## Hall of Fame

Security researchers who has helped make PlumbrC more secure:

*Be the first to report a vulnerability and get listed here!*
