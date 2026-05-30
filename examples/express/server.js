/**
 * PlumbrC + Express.js Integration Example
 * 
 * Runs PlumbrC as an HTTP sidecar and redacts all log output.
 * Start PlumbrC sidecar: ./build/bin/plumbr-server --port 8081
 * Then run: node server.js
 */

const express = require('express');
const app = express();

const PLUMBR_URL = process.env.PLUMBR_URL || 'http://localhost:8081';

// --- PlumbrC middleware: redact logs before they hit your logging pipeline ---
async function redactLog(message) {
  try {
    const res = await fetch(`${PLUMBR_URL}/api/redact`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ text: message }),
      signal: AbortSignal.timeout(2000),
    });
    if (!res.ok) return message;
    const data = await res.json();
    return data.redacted || message;
  } catch {
    return message; // Fail open: if PlumbrC is down, log unredacted
  }
}

// Override console.log to auto-redact
const originalLog = console.log;
console.log = async (...args) => {
  const message = args.map(a => typeof a === 'string' ? a : JSON.stringify(a)).join(' ');
  const safe = await redactLog(message);
  originalLog(safe);
};

// --- Express middleware: redact request/response logging ---
app.use(async (req, res, next) => {
  const start = Date.now();
  res.on('finish', async () => {
    const logLine = `${req.method} ${req.url} ${res.statusCode} ${Date.now() - start}ms`;
    await console.log(logLine);
  });
  next();
});

app.use(express.json());

// --- Example routes ---
app.post('/login', async (req, res) => {
  const { email, password } = req.body;
  // This log line would leak the password without PlumbrC!
  await console.log(`Login attempt: email=${email} password=${password}`);
  res.json({ status: 'ok' });
});

app.get('/health', (req, res) => {
  res.json({ status: 'healthy', redactor: PLUMBR_URL });
});

app.listen(3000, () => {
  originalLog('Express server running on http://localhost:3000');
  originalLog(`PlumbrC sidecar: ${PLUMBR_URL}`);
});
