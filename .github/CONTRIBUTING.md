# Contributing to PlumbrC

Thank you for your interest in contributing to PlumbrC! We welcome contributions from the community.

## Code of Conduct

By participating in this project, you agree to abide by our [Code of Conduct](CODE_OF_CONDUCT.md).

## How Can I Contribute?

### Reporting Bugs

Before creating bug reports, please check existing issues to avoid duplicates. When creating a bug report, include:

- **Clear title and description**
- **Steps to reproduce** the issue
- **Expected vs actual behavior**
- **Environment details** (OS, compiler version, etc.)
- **Code samples** if applicable

### Suggesting Features

Feature requests are welcome! Please:

- **Use a clear title** describing the feature
- **Provide detailed description** of the proposed functionality
- **Explain why** this feature would be useful
- **Include examples** of how it would work

### Pull Requests

1. **Fork the repo** and create your branch from `main`
2. **Make your changes** following our code style
3. **Add tests** if applicable
4. **Update documentation** as needed
5. **Ensure tests pass**: `make test`
6. **Submit your PR** with a clear description

## Development Setup

### Prerequisites

- GCC or Clang compiler
- libpcre2-dev
- Make
- (Optional) Python 3.7+ for Python package

### Building

```bash
# Clone the repository
git clone https://github.com/plumbr-io/plumbrC.git
cd plumbrC

# Build the library
make

# Run tests
make test

# Install (optional)
sudo make install
```

### Python Package Development

```bash
cd python
pip install -e ".[dev]"
pytest tests/
```

## Code Style

### C Code

- **Standard**: C11
- **Indentation**: 4 spaces (no tabs)
- **Line length**: 100 characters max
- **Naming**: 
  - Functions: `snake_case`
  - Structs: `PascalCase`
  - Constants: `UPPER_CASE`
- **Comments**: Use `//` for single-line, `/* */` for multi-line

### Python Code

- **Standard**: PEP 8
- **Indentation**: 4 spaces
- **Line length**: 100 characters max
- **Type hints**: Required for public APIs
- **Docstrings**: Google style

### Commit Messages

Follow conventional commits:

```
type(scope): subject

body (optional)

footer (optional)
```

**Types:**
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation changes
- `style`: Code style changes (formatting)
- `refactor`: Code refactoring
- `perf`: Performance improvements
- `test`: Adding/updating tests
- `chore`: Maintenance tasks

**Examples:**
```
feat(python): add batch processing API
fix(redactor): handle empty input correctly
docs(readme): update installation instructions
```

## Testing

### C Tests

```bash
make test
```

### Python Tests

```bash
cd python
pytest tests/ -v
pytest tests/ --cov=plumbrc  # with coverage
```

### Performance Tests

```bash
make benchmark
```

## Documentation

- Update README.md for user-facing changes
- Add docstrings for new functions
- Update API documentation if needed
- Include code examples for new features

## Review Process

1. **Automated checks** must pass (CI/CD)
2. **Code review** by maintainers
3. **Testing** on multiple platforms
4. **Merge** after approval

## Getting Help

- **GitHub Discussions**: For questions and ideas
- **Issues**: For bugs and feature requests
- **Discord**: For real-time chat (coming soon)

## Recognition

Contributors are recognized in:
- README.md contributors section
- Release notes
- Community spotlights

Thank you for contributing to PlumbrC! 🚀
