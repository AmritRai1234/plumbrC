# PlumbrC

**High-Performance Log Redaction Pipeline** - Pure C implementation targeting 500K+ lines/sec.

## Features

- **Two-phase matching**: Aho-Corasick literal scan + PCRE2 regex verification
- **Zero-allocation hot path**: Arena-based memory with no malloc in processing loop
- **Pipeline-friendly**: Designed for Unix stdin/stdout chaining
- **Built-in patterns**: Common secret detection (AWS keys, API tokens, etc.)
- **Safe C**: Bounds checking, input validation, sanitizer-ready

## Quick Start

```bash
# Build
make

# Run
./build/bin/plumbr < app.log > redacted.log

# Pipeline usage
tail -f /var/log/app.log | ./build/bin/plumbr | tee redacted.log
```

## Build Options

```bash
make            # Optimized release build
make debug      # Debug build with symbols
make sanitize   # Build with AddressSanitizer/UBSan
make profile    # Build for gprof profiling
make benchmark  # Run performance benchmark
```

## Usage

```
plumbr [OPTIONS] < input > output

Options:
  -p, --patterns FILE   Load patterns from FILE
  -d, --defaults        Use built-in default patterns (default: on)
  -D, --no-defaults     Disable built-in default patterns
  -q, --quiet           Suppress statistics output
  -s, --stats           Print statistics to stderr
  -h, --help            Show help
  -v, --version         Show version
```

## Pattern File Format

One pattern per line:
```
name|literal|regex|replacement
```

Example:
```
aws_key|AKIA|AKIA[0-9A-Z]{16}|[REDACTED:aws_key]
password|password|password[=:]\s*\S+|[REDACTED:password]
```

## Architecture

```
stdin → [Read Buffer 64KB] → [Line Parser] → [Aho-Corasick] → [PCRE2 Verify] → [Redact] → [Write Buffer 64KB] → stdout
```

### Performance Optimizations

1. **Aho-Corasick first pass**: O(n) scan for all patterns simultaneously
2. **PCRE2 JIT**: Only on lines with literal matches (~1-5% of traffic)
3. **Arena allocation**: Single mmap, no malloc/free during processing
4. **Cache-friendly I/O**: 64KB buffers aligned to L2 cache
5. **Vectorized writes**: writev() for batch output

## Dependencies

- GCC or Clang (C11)
- PCRE2 library (`apt install libpcre2-dev`)

## License

MIT
