#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
PID_DIR="${PROJECT_DIR}/.pids"

echo "=== Stopping DANN Distributed System ==="

stop_service() {
    local name=$1
    local pid_file="${PID_DIR}/${name}.pid"

    if [ -f "$pid_file" ]; then
        local pid=$(cat "$pid_file")
        if kill -0 "$pid" 2>/dev/null; then
            kill "$pid"
            echo "Stopped $name (PID: $pid)"
        else
            echo "$name was not running"
        fi
        rm -f "$pid_file"
    else
        echo "$name: no PID file found"
    fi
}

stop_service "shard_0"
stop_service "shard_1"

echo ""
echo "=== All services stopped ==="
