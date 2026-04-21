#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build"
DATA_DIR="${PROJECT_DIR}/data/distributed_index"
LOG_DIR="${PROJECT_DIR}/logs"
PID_DIR="${PROJECT_DIR}/.pids"

SHARD_COUNT=2
MASTER_GRPC_PORT=50051

SHARD_ADDRESSES=""
for ((i=0; i<SHARD_COUNT; i++)); do
    PORT=$((50052 + i))
    if [ -n "$SHARD_ADDRESSES" ]; then
        SHARD_ADDRESSES="${SHARD_ADDRESSES},"
    fi
    SHARD_ADDRESSES="${SHARD_ADDRESSES}localhost:${PORT}"
done

echo "=== Starting DANN Distributed System ==="
echo ""
echo "This will:"
echo "  1. Start shard 0 node on port 50052"
echo "  2. Start shard 1 node on port 50053"
echo "  3. Start master gateway node on port 50051"
echo ""

rm -rf "${DATA_DIR}"
mkdir -p "${DATA_DIR}"
mkdir -p "${LOG_DIR}"
mkdir -p "${PID_DIR}"

if [ -f "${PID_DIR}/master.pid" ]; then
    kill $(cat "${PID_DIR}/master.pid") 2>/dev/null || true
fi
for ((i=0; i<SHARD_COUNT; i++)); do
    if [ -f "${PID_DIR}/shard_${i}.pid" ]; then
        kill $(cat "${PID_DIR}/shard_${i}.pid") 2>/dev/null || true
    fi
done

cd "${BUILD_DIR}"

echo "=== Step 1: Building index (master build-only phase) ==="
./dann_server \
    --role master \
    --node-id master \
    --dimension 128 \
    --shards ${SHARD_COUNT} \
    --num-vectors 10000 \
    --persistence-path "${DATA_DIR}"

echo ""
echo "=== Step 2: Starting shard nodes in background ==="
echo ""

for ((i=0; i<SHARD_COUNT; i++)); do
    PORT=$((50052 + i))
    NODE_ID="shard_${i}"
    nohup ./dann_server \
        --role shard \
        --node-id "${NODE_ID}" \
        --shard-id ${i} \
        --shards ${SHARD_COUNT} \
        --dimension 128 \
        --grpc-port ${PORT} \
        --persistence-path "${DATA_DIR}" \
        > "${LOG_DIR}/shard_${i}.log" 2>&1 &
    PID=$!
    echo $PID > "${PID_DIR}/shard_${i}.pid"
    echo "Shard ${i} started (PID: ${PID}), port: ${PORT}, logs: ${LOG_DIR}/shard_${i}.log"
    sleep 1
done

echo ""
echo "=== Step 3: Starting master gateway node in background ==="
echo ""

nohup ./dann_server \
    --role master \
    --node-id master \
    --dimension 128 \
    --shards ${SHARD_COUNT} \
    --grpc-port ${MASTER_GRPC_PORT} \
    --shard-addresses "${SHARD_ADDRESSES}" \
    --persistence-path "${DATA_DIR}" \
    > "${LOG_DIR}/master.log" 2>&1 &
MASTER_PID=$!
echo $MASTER_PID > "${PID_DIR}/master.pid"
echo "Master gateway started (PID: ${MASTER_PID}), port: ${MASTER_GRPC_PORT}, logs: ${LOG_DIR}/master.log"

echo ""
echo "=== Distributed System Started ==="
echo "Master gateway: localhost:${MASTER_GRPC_PORT}"
for ((i=0; i<SHARD_COUNT; i++)); do
    PORT=$((50052 + i))
    echo "Shard ${i}: localhost:${PORT}"
done
echo ""
echo "To query, connect to master at localhost:${MASTER_GRPC_PORT}"
echo "To view logs: tail -f ${LOG_DIR}/*.log"
echo "To stop: ${SCRIPT_DIR}/stop_all.sh"
