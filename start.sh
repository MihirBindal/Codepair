#!/bin/bash

# Ensure Redis is running (either systemd or container)
if ! redis-cli ping >/dev/null 2>&1; then
    echo "Starting Redis container..."
    docker start codepair-redis >/dev/null 2>&1 || docker run -d --name codepair-redis -p 6379:6379 redis:alpine
fi

# Start Sidecar in background
echo "Starting FastAPI Sidecar on port 8000..."
cd sidecar
./../venv/bin/uvicorn main:app --host 127.0.0.1 --port 8000 > sidecar.log 2>&1 &
SIDECAR_PID=$!
cd ..

# Start C++ Server in background
echo "Starting C++ CodePair Server on port 9001..."
./build/codepair_server > server.log 2>&1 &
SERVER_PID=$!

# Start Vite React Frontend in background
echo "Starting React Frontend on port 5173..."
cd frontend
npm run dev > dev.log 2>&1 &
FRONTEND_PID=$!
cd ..

# Save PIDs to a file for stopping later
echo "$SIDECAR_PID" > .pids
echo "$SERVER_PID" >> .pids
echo "$FRONTEND_PID" >> .pids

echo "------------------------------------------------"
echo "CodePair Application is now RUNNING!"
echo "- Frontend: http://localhost:5173"
echo "- C++ Server: port 9001 (WS)"
echo "- Sidecar: port 8000"
echo "------------------------------------------------"
echo "To stop the app, run: ./stop.sh"
