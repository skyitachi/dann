#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build"
DATA_DIR="${PROJECT_DIR}/data/distributed_index"

echo "=== Starting 3-Node DANN Distributed System ==="
echo ""
echo "This will:"
echo "  1. Run master node to build and save index"
echo "  2. Start shard 0 node on port 50052"
echo "  3. Start shard 1 node on port 50053"
echo ""

rm -rf "${DATA_DIR}"
mkdir -p "${DATA_DIR}"

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
echo "=== Master completed. Starting shard nodes in separate terminals ==="
echo ""

osascript <<EOF
tell application "Terminal"
    activate
    do script "cd '${BUILD_DIR}' && ./dann_server --role shard --node-id shard_0 --shard-id 0 --shards 2 --dimension 128 --grpc-port 50052 --persistence-path '${DATA_DIR}'"
    
    delay 1
    
    tell application "System Events"
        keystroke "t" using {command down}
    end tell
    
    delay 1
    
    do script "cd '${BUILD_DIR}' && ./dann_server --role shard --node-id shard_1 --shard-id 1 --shards 2 --dimension 128 --grpc-port 50053 --persistence-path '${DATA_DIR}'" in window 2
end tell
EOF

echo ""
echo "=== Distributed System Started ==="
echo "Shard 0: localhost:50052"
echo "Shard 1: localhost:50053"
echo ""
echo "You can test with gRPC client or use test_grpc_client"
