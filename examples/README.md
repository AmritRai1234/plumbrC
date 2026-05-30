# PlumbrC Integration Examples

These examples show how to integrate PlumbrC's log redaction into any server.

## Architecture

PlumbrC runs as a **sidecar** — a small HTTP server that your app calls to redact log lines before they reach your logging pipeline.

```
Your App ──→ PlumbrC Sidecar ──→ Clean Logs ──→ ELK / Datadog / CloudWatch
              (port 8081)           (no secrets)
```

## Quick Start with Docker Compose

```bash
cd examples
docker compose up
```

This starts PlumbrC + an Express.js example app. Try:

```bash
curl -X POST http://localhost:3000/login \
  -H 'Content-Type: application/json' \
  -d '{"email": "user@test.com", "password": "SuperSecret123"}'
```

Check the logs — the password is redacted!

## Framework Examples

| Framework | Language | File |
|-----------|----------|------|
| Express.js | Node.js | [express/server.js](express/server.js) |
| Flask | Python | [flask/app.py](flask/app.py) |
| Go net/http | Go | [go/main.go](go/main.go) |
| C Library | C | [c/example.c](c/example.c) |
| Python ctypes | Python | [python/example.py](python/example.py) |

## Integration Pattern

All examples follow the same pattern:

1. **Start PlumbrC sidecar**: `./build/bin/plumbr-server --port 8081`
2. **Send logs to PlumbrC**: `POST http://localhost:8081/api/redact` with `{"text": "your log line"}`
3. **Use the redacted result**: Write to your logging pipeline

### Middleware Approach (Recommended)

Wrap your logger so all output is automatically redacted:

```javascript
// Node.js
const originalLog = console.log;
console.log = async (...args) => {
  const message = args.join(' ');
  const safe = await redact(message);
  originalLog(safe);
};
```

```python
# Python
class PlumbrLogHandler(logging.Handler):
    def emit(self, record):
        msg = self.format(record)
        safe = requests.post(f'{PLUMBR_URL}/api/redact', json={'text': msg}).json()['redacted']
        print(safe)
```

```go
// Go
log.SetOutput(&RedactingWriter{client: &http.Client{Timeout: 2 * time.Second}})
```

## Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `PLUMBR_URL` | `http://localhost:8081` | PlumbrC sidecar URL |
| `PLUMBR_DATA_DIR` | *(auto-detect)* | Path to PlumbrC patterns directory |
