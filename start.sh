#!/bin/sh
echo "Starting PlumbrC HTTP Server on port 8081..."
/app/build/bin/plumbr-server --port 8081 --threads ${PLUMBR_THREADS:-4} &
PLUMBR_PID=$!

echo "Starting Next.js on port ${PORT:-3000}..."
cd /app/web && node server.js &
NEXT_PID=$!

# Handle signals
trap "kill $PLUMBR_PID $NEXT_PID; exit 0" SIGTERM SIGINT

# Wait for either to exit
wait -n $PLUMBR_PID $NEXT_PID
kill $PLUMBR_PID $NEXT_PID 2>/dev/null
