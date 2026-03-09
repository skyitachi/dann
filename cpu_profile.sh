./build/faiss_bench &
BENCH_PID=$!
# 等待初始化（进入搜索阶段）
echo "Waiting for benchmark to initialize (5s)..."
sleep 5
# 开始60秒 perf 采样
echo "Starting perf sampling for 60 seconds..."
perf record -o perf_faiss.data -e cycles,instructions,cache-references,cache-misses,branches,branch-misses \
    -F 99 -p $BENCH_PID -- sleep 60