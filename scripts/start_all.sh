#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build"
DATA_DIR="${PROJECT_DIR}/data/distributed_index"
LOG_DIR="${PROJECT_DIR}/logs"
PID_DIR="${PROJECT_DIR}/.pids"

echo "=== Starting 3-Node DANN Distributed System ==="
echo ""
echo "This will:"
echo "  1. Run master node to build and save index"
echo "  2. Start shard 0 node on port 50052"
echo "  3. Start shard 1 node on port 50053"
echo ""

# Clean up and create directories
rm -rf "${DATA_DIR}"
mkdir -p "${DATA_DIR}"
mkdir -p "${LOG_DIR}"
mkdir -p "${PID_DIR}"

# Clean up existing processes if any
if [ -f "${PID_DIR}/shard_0.pid" ]; then
    kill $(cat "${PID_DIR}/shard_0.pid") 2>/dev/null || true
fi
if [ -f "${PID_DIR}/shard_1.pid" ]; then
    kill $(cat "${PID_DIR}/shard_1.pid") 2>/dev/null || true
fi

cd "${BUILD_DIR}"

echo "=== Step 1: Running Master Node (build & save) ==="
echo ""

./dann_server \
    --role master \
    --node-id master \
    --dimension 128 \
    --shards 2 \
    --num-vectors 10000 \
    --persistence-path "${DATA_DIR}"

echo ""
echo "=== Master completed. Starting shard nodes in background ==="
echo ""

# Start shard 0
nohup ./dann_server \
    --role shard \
    --node-id shard_0 \
    --shard-id 0 \
    --shards 2 \
    --dimension 128 \
    --grpc-port 50052 \
    --persistence-path "${DATA_DIR}" \
    > "${LOG_DIR}/shard_0.log" 2>&1 &
SHARD0_PID=$!
echo $SHARD0_PID > "${PID_DIR}/shard_0.pid"
echo "Shard 0 started (PID: $SHARD0_PID), logs: ${LOG_DIR}/shard_0.log"

sleep 1

# Start shard 1
nohup ./dann_server \
    --role shard \
    --node-id shard_1 \
    --shard-id 1 \
    --shards 2 \
    --dimension 128 \
    --grpc-port 50053 \
    --persistence-path "${DATA_DIR}" \
    > "${LOG_DIR}/shard_1.log" 2>&1 &
SHARD1_PID=$!
echo $SHARD1_PID > "${PID_DIR}/shard_1.pid"
echo "Shard 1 started (PID: $SHARD1_PID), logs: ${LOG_DIR}/shard_1.log"

echo ""
echo "=== Distributed System Started ==="
echo "Shard 0: localhost:50052"
echo "Shard 1: localhost:50053"
echo ""
echo "Log files:"
echo "  ${LOG_DIR}/shard_0.log"
echo "  ${LOG_DIR}/shard_1.log"
echo ""
echo "To view logs: tail -f ${LOG_DIR}/shard_*.log"
echo "To stop: ${SCRIPT_DIR}/stop_all.sh"
