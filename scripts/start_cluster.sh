#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.. && pwd)"
BUILD_DIR="${PROJECT_DIR}/build"
DATA_DIR="${PROJECT_DIR}/data/distributed_index"

echo "=== Starting 3-Node DANN Distributed System ==="
echo ""
echo "This script starts:"
echo "  1. Master node (builds index and saves shards)"
echo "  2. Shard 0 node (loads shard_0 and serves queries)"
echo "  3. Shard 1 node (loads shard_1 and serves queries)"
echo ""

rm -rf "${DATA_DIR}"
mkdir -p "${DATA_DIR}"

cd "${BUILD_DIR}"

echo "=== Step 1: Running Master Node ==="
./dann_server \
    --role master \
    --node-id master \
    --dimension 128 \
    --shards 2 \
    --num-vectors 10000 \
    --persistence-path "${DATA_DIR}"

echo ""
echo "=== Step 2: Starting Shard Nodes ==="
echo "Starting shard 0 on port 50052..."
gnome-terminal --title="DANN Shard 0" -- ./dann_server \
    --role shard \
    --node-id shard_0 \
    --shard-id 0 \
    --shards 2 \
    --dimension 128 \
    --grpc-port 50052 \
    --persistence-path "${DATA_DIR}" 2>/dev/null || \
osascript -e 'tell application "Terminal" to do script "cd '"${BUILD_DIR}"' && ./dann_server --role shard --node-id shard_0 --shard-id 0 --shards 2 --dimension 128 --grpc-port 50052 --persistence-path '"${DATA_DIR}"'"' 2>/dev/null || \
xterm -title "DANN Shard 0" -e "./dann_server --role shard --node-id shard_0 --shard-id 0 --shards 2 --dimension 128 --grpc-port 50052 --persistence-path ${DATA_DIR}" 2>/dev/null || \
echo "Please run manually: ./dann_server --role shard --node-id shard_0 --shard-id 0 --shards 2 --dimension 128 --grpc-port 50052 --persistence-path ${DATA_DIR}"

sleep 2

echo "Starting shard 1 on port 50053..."
gnome-terminal --title="DANN Shard 1" -- ./dann_server \
    --role shard \
    --node-id shard_1 \
    --shard-id 1 \
    --shards 2 \
    --dimension 128 \
    --grpc-port 50053 \
    --persistence-path "${DATA_DIR}" 2>/dev/null || \
osascript -e 'tell application "Terminal" to do script "cd '"${BUILD_DIR}"' && ./dann_server --role shard --node-id shard_1 --shard-id 1 --shards 2 --dimension 128 --grpc-port 50053 --persistence-path '"${DATA_DIR}"'"' 2>/dev/null || \
xterm -title "DANN Shard 1" -e "./dann_server --role shard --node-id shard_1 --shard-id 1 --shards 2 --dimension 128 --grpc-port 50053 --persistence-path ${DATA_DIR}" 2>/dev/null || \
echo "Please run manually: ./dann_server --role shard --node-id shard_1 --shard-id 1 --shards 2 --dimension 128 --grpc-port 50053 --persistence-path ${DATA_DIR}"

echo ""
echo "=== Distributed System Started ==="
echo "Shard 0: localhost:50052"
echo "Shard 1: localhost:50053"
