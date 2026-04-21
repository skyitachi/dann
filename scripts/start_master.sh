#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build"
DATA_DIR="${PROJECT_DIR}/data/distributed_index"

SHARD_COUNT=${1:-2}
MASTER_GRPC_PORT=50051

SHARD_ADDRESSES=""
for ((i=0; i<SHARD_COUNT; i++)); do
    PORT=$((50052 + i))
    if [ -n "$SHARD_ADDRESSES" ]; then
        SHARD_ADDRESSES="${SHARD_ADDRESSES},"
    fi
    SHARD_ADDRESSES="${SHARD_ADDRESSES}localhost:${PORT}"
done

echo "=== Starting Master Node (Gateway) ==="
echo "Project dir: ${PROJECT_DIR}"
echo "Data dir: ${DATA_DIR}"
echo "gRPC port: ${MASTER_GRPC_PORT}"
echo "Shard addresses: ${SHARD_ADDRESSES}"

mkdir -p "${DATA_DIR}"

cd "${BUILD_DIR}"

./dann_server \
    --role master \
    --node-id master \
    --dimension 128 \
    --shards ${SHARD_COUNT} \
    --num-vectors 10000 \
    --grpc-port ${MASTER_GRPC_PORT} \
    --shard-addresses "${SHARD_ADDRESSES}" \
    --persistence-path "${DATA_DIR}"

echo "Master node finished."
