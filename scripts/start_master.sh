#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build"
DATA_DIR="${PROJECT_DIR}/data/distributed_index"

echo "=== Starting Master Node ==="
echo "Project dir: ${PROJECT_DIR}"
echo "Data dir: ${DATA_DIR}"

mkdir -p "${DATA_DIR}"

cd "${BUILD_DIR}"

./dann_server \
    --role master \
    --node-id master \
    --dimension 128 \
    --shards 2 \
    --num-vectors 10000 \
    --persistence-path "${DATA_DIR}"

echo "Master node finished."
