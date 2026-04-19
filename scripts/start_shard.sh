#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build"
DATA_DIR="${PROJECT_DIR}/data/distributed_index"

SHARD_ID=${1:-0}
GRPC_PORT=$((50052 + SHARD_ID))
NODE_ID="shard_${SHARD_ID}"

echo "=== Starting Shard ${SHARD_ID} Node ==="
echo "Project dir: ${PROJECT_DIR}"
echo "Data dir: ${DATA_DIR}"
echo "gRPC port: ${GRPC_PORT}"

cd "${BUILD_DIR}"

./dann_server \
    --role shard \
    --node-id "${NODE_ID}" \
    --shard-id ${SHARD_ID} \
    --shards 2 \
    --dimension 128 \
    --grpc-port ${GRPC_PORT} \
    --persistence-path "${DATA_DIR}"
