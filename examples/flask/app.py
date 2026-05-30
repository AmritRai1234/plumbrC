"""
PlumbrC + Flask Integration Example

Runs PlumbrC as an HTTP sidecar and redacts all log output.
Start PlumbrC sidecar: ./build/bin/plumbr-server --port 8081
Then run: python app.py
"""

import logging
import os
import requests
from flask import Flask, request, jsonify

PLUMBR_URL = os.environ.get('PLUMBR_URL', 'http://localhost:8081')


class PlumbrLogHandler(logging.Handler):
    """Custom log handler that redacts secrets via PlumbrC sidecar."""

    def __init__(self, plumbr_url=PLUMBR_URL):
        super().__init__()
        self.plumbr_url = plumbr_url
        self.session = requests.Session()

    def emit(self, record):
        msg = self.format(record)
        try:
            resp = self.session.post(
                f'{self.plumbr_url}/api/redact',
                json={'text': msg},
                timeout=2,
            )
            if resp.ok:
                msg = resp.json().get('redacted', msg)
        except requests.RequestException:
            pass  # Fail open: log unredacted if sidecar is down
        # Write the (possibly redacted) message to stderr
        print(msg)


# Set up redacting logger
logger = logging.getLogger('app')
logger.setLevel(logging.INFO)
logger.addHandler(PlumbrLogHandler())

app = Flask(__name__)


@app.route('/login', methods=['POST'])
def login():
    data = request.get_json()
    email = data.get('email', '')
    password = data.get('password', '')
    # This log line would leak the password without PlumbrC!
    logger.info(f'Login attempt: email={email} password={password}')
    return jsonify(status='ok')


@app.route('/health')
def health():
    return jsonify(status='healthy', redactor=PLUMBR_URL)


if __name__ == '__main__':
    logger.info('Flask server starting with PlumbrC redaction')
    app.run(port=5000, debug=True)
