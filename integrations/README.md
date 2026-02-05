# PlumbrC Integrations

Ready-to-use configurations for seamless integration with popular log pipelines.

## Quick Start

```bash
# Install PlumbrC
make && sudo make install

# Verify installation
plumbr --version
```

## Available Integrations

| Platform | Directory | Description |
|----------|-----------|-------------|
| Docker | `docker/` | Dockerfile, docker-compose examples |
| Kubernetes | `kubernetes/` | Sidecar, DaemonSet, init container |
| Fluentd | `fluentd/` | Filter plugin configuration |
| Logstash | `logstash/` | Pipeline filter configuration |
| Vector | `vector/` | Transform configuration |
| systemd | `systemd/` | Service unit files |

## Usage Patterns

### 1. Pipe Mode (simplest)
```bash
your-app | plumbr | destination
```

### 2. File Mode
```bash
plumbr < input.log > output.log
```

### 3. Sidecar Mode
Run alongside your application in the same pod/container.

### 4. DaemonSet Mode
Run one instance per node, process all container logs.

## Pattern Selection

Load specific pattern sets for your use case:

```bash
# Database-focused redaction
plumbr -p patterns/database/postgresql.txt

# All patterns
plumbr -p patterns/all.txt

# Default patterns (built-in)
plumbr
```
