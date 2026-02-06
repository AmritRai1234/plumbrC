# Contributing to PlumbrC

Thank you for your interest in contributing to PlumbrC! This document provides guidelines and instructions for contributing.

## 📋 Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Setup](#development-setup)
- [Coding Standards](#coding-standards)
- [Testing](#testing)
- [Submitting Changes](#submitting-changes)
- [Reporting Bugs](#reporting-bugs)
- [Requesting Features](#requesting-features)

## Code of Conduct

Please be respectful and constructive in all interactions. We welcome contributors of all experience levels.

## Getting Started

1. **Fork the repository** on GitHub
2. **Clone your fork** locally:
   ```bash
   git clone https://github.com/YOUR_USERNAME/plumbrC.git
   cd plumbrC
   ```
3. **Add upstream remote**:
   ```bash
   git remote add upstream https://github.com/ORIGINAL_OWNER/plumbrC.git
   ```

## Development Setup

### Dependencies

```bash
# Debian/Ubuntu
sudo apt install build-essential libpcre2-dev clang-format cppcheck valgrind

# macOS
brew install pcre2 clang-format cppcheck
```

### Building

```bash
# Release build
make

# Debug build (for development)
make debug

# With sanitizers (recommended for testing)
make sanitize
```

### Verify Setup

```bash
# Run quick test
make test

# Check dependencies
make check-deps
```

## Coding Standards

### C Style Guidelines

- **Standard**: C11 (`-std=c11`)
- **Indentation**: 2 spaces (no tabs)
- **Line length**: 80 characters max
- **Braces**: K&R style (opening brace on same line)

### Formatting

Use `clang-format` before committing:

```bash
make format
```

### Naming Conventions

| Type | Convention | Example |
|------|------------|---------|
| Functions | `snake_case` | `ac_add_pattern()` |
| Types/Structs | `PascalCase` | `ACAutomaton` |
| Constants | `SCREAMING_SNAKE_CASE` | `PLUMBR_MAX_LINE_SIZE` |
| Local variables | `snake_case` | `pattern_count` |
| File-static functions | `snake_case` with `static` | `static int helper_func()` |

### Documentation

- All public functions must have header comments
- Use `/* */` comments for multi-line, `//` for single-line
- Document non-obvious logic inline

### Error Handling

- Check all return values
- Validate all input parameters
- Use bounds checking for buffer operations
- Avoid memory leaks (use arena allocator when possible)

## Testing

### Running Tests

```bash
# Quick functionality test
make test

# Unit tests
make test-unit

# With sanitizers (catch memory issues)
make sanitize
./build/bin/plumbr < test_data/sample.log > /dev/null

# Benchmark
make benchmark
```

### Writing Tests

- Add unit tests for new functionality
- Test edge cases (empty input, max sizes, etc.)
- Ensure tests pass with sanitizers enabled

### Static Analysis

```bash
make analyze
```

## Submitting Changes

### Workflow

1. **Create a feature branch**:
   ```bash
   git checkout -b feature/your-feature-name
   ```

2. **Make your changes**:
   - Write clear, focused commits
   - Keep commits atomic (one logical change per commit)

3. **Test thoroughly**:
   ```bash
   make sanitize
   make test
   make test-unit
   ```

4. **Format code**:
   ```bash
   make format
   ```

5. **Push to your fork**:
   ```bash
   git push origin feature/your-feature-name
   ```

6. **Open a Pull Request** on GitHub

### Commit Messages

Format:
```
<type>: <short description>

<optional longer description>
```

Types:
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation changes
- `style`: Code style changes (formatting, etc.)
- `refactor`: Code refactoring
- `test`: Adding or updating tests
- `perf`: Performance improvements
- `chore`: Maintenance tasks

Examples:
```
feat: add support for custom replacement functions

fix: prevent buffer overflow in redactor_process

docs: update README with library usage examples
```

### Pull Request Guidelines

- Provide a clear description of changes
- Reference any related issues
- Ensure all tests pass
- Update documentation if needed
- Keep changes focused and reviewable

## Reporting Bugs

When reporting bugs, please include:

1. **PlumbrC version** (`plumbr --version`)
2. **Operating system** and version
3. **Steps to reproduce**
4. **Expected behavior**
5. **Actual behavior**
6. **Minimal test case** (if possible)

### Security Issues

For security vulnerabilities, please do NOT open a public issue. Instead, contact the maintainers directly (see LICENSE for contact info).

## Requesting Features

Feature requests are welcome! Please:

1. **Search existing issues** first to avoid duplicates
2. **Describe the use case** — why is this feature needed?
3. **Propose a solution** if you have one in mind
4. Be willing to help implement if possible

## Adding Patterns

To contribute new detection patterns:

1. **Choose the appropriate category** in `patterns/`
2. **Follow the pattern format**:
   ```
   name|literal|regex|replacement
   ```
3. **Test your pattern**:
   ```bash
   echo "test string" | ./build/bin/plumbr -p your_patterns.txt
   ```
4. **Ensure no false positives** on common text
5. **Update counts** in `patterns/README.md`

## License

By contributing, you agree that your contributions will be licensed under the same [Source Available License](LICENSE) as the project.

---

Thank you for contributing to PlumbrC! 🔧
