# L2_distance 性能优化建议

## 当前实现 (src/utils/util.cpp:12)
```cpp
float L2_distance(const float* x, const float* y, int d) {
    float dist = 0.0f;
    for (int i = 0; i < d; i++) {
        float diff = x[i] - y[i];
        dist += diff * diff;
    }
    return dist;
}
```

## 性能问题
- 98.63% CPU时间消耗于此函数
- 简单的串行循环，未利用SIMD指令
- 256维向量的距离计算可以大幅向量化

## 优化方案

### 1. SIMD优化 (使用AVX2/AVX-512)
```cpp
#include <immintrin.h>

float L2_distance_avx2(const float* x, const float* y, int d) {
    __m256 sum = _mm256_setzero_ps();
    int i = 0;
    
    // 处理8个浮点数的块 (256位 = 8 x 32位)
    for (; i + 8 <= d; i += 8) {
        __m256 vx = _mm256_loadu_ps(x + i);
        __m256 vy = _mm256_loadu_ps(y + i);
        __m256 diff = _mm256_sub_ps(vx, vy);
        __m256 squared = _mm256_mul_ps(diff, diff);
        sum = _mm256_add_ps(sum, squared);
    }
    
    // 水平求和
    __m128 low = _mm256_castps256_ps128(sum);
    __m128 high = _mm256_extractf128_ps(sum, 1);
    __m128 result = _mm_add_ps(low, high);
    result = _mm_hadd_ps(result, result);
    result = _mm_hadd_ps(result, result);
    
    float distance = _mm_cvtss_f32(result);
    
    // 处理剩余元素
    for (; i < d; i++) {
        float diff = x[i] - y[i];
        distance += diff * diff;
    }
    
    return distance;
}
```

### 2. 使用FAISS的优化版本
```cpp
// FAISS已经提供了高度优化的距离计算
#include <faiss/utils/distances.h>

float L2_distance_faiss(const float* x, const float* y, int d) {
    float dist = 0.0f;
    // FAISS的L2距离计算使用了SIMD和多线程优化
    faiss::fvec_L2sqr(&dist, x, y, d);
    return dist;
}
```

### 3. 简单的编译器向量化
```cpp
// 添加编译器提示，鼓励自动向量化
float L2_distance_vec(const float* x, const float* y, int d) {
    float dist = 0.0f;
    #pragma omp simd reduction(+:dist)
    for (int i = 0; i < d; i++) {
        float diff = x[i] - y[i];
        dist += diff * diff;
    }
    return dist;
}
```

## 预期性能提升
- AVX2: 4-8倍加速 (256位SIMD)
- AVX-512: 8-16倍加速
- FAISS: 10-20倍加速 (包含多线程优化)

## 实施建议
1. 先使用FAISS的版本 (最简单)
2. 如果需要定制化，实现AVX2版本
3. 运行基准测试对比 performance

## 测试命令
```bash
# 对比优化前后的性能
perf stat -e cycles,instructions,L1-dcache-load-misses ./benchmark/dann_bench_orig
perf stat -e cycles,instructions,L1-dcache-load-misses ./benchmark/dann_bench_opt
```
