#!/bin/bash

echo "Stopping CodePair Application services..."

# 1. Kill recorded PIDs if the file exists
if [ -f .pids ]; then
    while read -r pid; do
        if kill -0 "$pid" >/dev/null 2>&1; then
            echo "Stopping process $pid..."
            kill -9 "$pid"
        fi
    done < .pids
    rm -f .pids
fi

# 2. Always double-check and kill any processes listening on the application ports
PORTS=(4002 4001 4000)
for port in "${PORTS[@]}"; do
    PIDS=$(lsof -t -i:"$port")
    if [ ! -z "$PIDS" ]; then
        for pid in $PIDS; do
            echo "Stopping lingering process on port $port (PID $pid)..."
            kill -9 "$pid" 2>/dev/null
        done
    fi
done

# 3. Stop Redis container if running
if docker ps | grep -q codepair-redis; then
    echo "Stopping Redis container..."
    docker stop codepair-redis >/dev/null
fi

echo "All services stopped."
