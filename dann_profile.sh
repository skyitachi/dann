#!/bin/bash
set -e

cd "$(dirname "$0")"

# 运行 dann_bench 并在搜索阶段进行 perf 采样
./build/dann_bench &
BENCH_PID=$!

# 等待初始化（进入搜索阶段）
echo "Waiting for benchmark to initialize (10s)..."
sleep 10

# 开始60秒 perf 采样
echo "Starting perf sampling for 60 seconds..."
perf record -o perf_dann.data -e cycles,instructions,cache-references,cache-misses,branches,branch-misses \
    -F 99 -p $BENCH_PID -- sleep 60

echo "Profile complete. Data saved to perf_dann.data"
echo "Run 'perf report -i perf_dann.data' to view results"
