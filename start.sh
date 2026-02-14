#!/bin/sh
# PlumbrC Production Startup
# nginx :3000 → C server :8081 (redact) + Next.js :3001 (web app)

echo "Starting PlumbrC HTTP Server on port 8081..."
/app/build/bin/plumbr-server --port 8081 --threads ${PLUMBR_THREADS:-4} &
PLUMBR_PID=$!

echo "Starting Next.js on port 3001 (internal)..."
cd /app/web && PORT=3001 node server.js &
NEXT_PID=$!

# Wait for backends to be ready
sleep 2

echo "Starting nginx on port 3000 (public)..."
nginx -g 'daemon off;' &
NGINX_PID=$!

echo ""
echo "╔══════════════════════════════════════════╗"
echo "║        PlumbrC Ready                     ║"
echo "╠══════════════════════════════════════════╣"
echo "║  nginx     :3000  (public entry point)   ║"
echo "║  C server  :8081  /api/redact (direct)   ║"
echo "║  Next.js   :3001  dashboard (internal)   ║"
echo "╚══════════════════════════════════════════╝"
echo ""

# Handle signals — graceful shutdown
trap "kill $NGINX_PID $PLUMBR_PID $NEXT_PID 2>/dev/null; exit 0" SIGTERM SIGINT

# Wait for any process to exit
wait -n $NGINX_PID $PLUMBR_PID $NEXT_PID
kill $NGINX_PID $PLUMBR_PID $NEXT_PID 2>/dev/null
