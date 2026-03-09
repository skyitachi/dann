// L2_distance 优化实现示例
// 文件: src/utils/util.cpp

#include "dann/utils.h"
#include <algorithm>
#include <queue>

#ifdef __AVX2__
#include <immintrin.h>
#endif

namespace dann
{

// ===== 原始实现 (性能: 1x) =====
float L2_distance_original(const float* x, const float* y, int d) {
    float dist = 0.0f;
    for (int i = 0; i < d; i++) {
        float diff = x[i] - y[i];
        dist += diff * diff;
    }
    return dist;
}

// ===== 方案1: FAISS优化 (性能: 10-20x) =====
#include <faiss/utils/distances.h>

float L2_distance_faiss(const float* x, const float* y, int d) {
    return faiss::fvec_L2sqr(x, y, d);
}

// ===== 方案2: OpenMP向量化 (性能: 2-4x) =====
float L2_distance_openmp(const float* x, const float* y, int d) {
    float dist = 0.0f;
    #pragma omp simd reduction(+:dist) aligned(x,y:32)
    for (int i = 0; i < d; i++) {
        float diff = x[i] - y[i];
        dist += diff * diff;
    }
    return dist;
}

// ===== 方案3: AVX2优化 (性能: 4-8x) =====
#ifdef __AVX2__
static inline float horizontal_sum_avx2(__m256 v) {
    __m128 hi = _mm256_extractf128_ps(v, 1);
    __m128 lo = _mm256_castps256_ps128(v);
    __m128 sum = _mm_add_ps(hi, lo);
    sum = _mm_hadd_ps(sum, sum);
    sum = _mm_hadd_ps(sum, sum);
    return _mm_cvtss_f32(sum);
}

float L2_distance_avx2(const float* x, const float* y, int d) {
    __m256 sum = _mm256_setzero_ps();
    int i = 0;
    
    // 8个float并行处理 (256位SIMD)
    for (; i + 8 <= d; i += 8) {
        __m256 vx = _mm256_loadu_ps(x + i);
        __m256 vy = _mm256_loadu_ps(y + i);
        __m256 diff = _mm256_sub_ps(vx, vy);
        __m256 squared = _mm256_mul_ps(diff, diff);
        sum = _mm256_add_ps(sum, squared);
    }
    
    // 水平求和
    float distance = horizontal_sum_avx2(sum);
    
    // 处理剩余元素
    for (; i < d; i++) {
        float diff = x[i] - y[i];
        distance += diff * diff;
    }
    
    return distance;
}
#endif

// ===== 方案4: 展开优化 (性能: 1.5-2x) =====
float L2_distance_unrolled(const float* x, const float* y, int d) {
    float dist = 0.0f;
    int i = 0;
    
    // 4次展开
    for (; i + 4 <= d; i += 4) {
        float diff0 = x[i] - y[i];
        float diff1 = x[i+1] - y[i+1];
        float diff2 = x[i+2] - y[i+2];
        float diff3 = x[i+3] - y[i+3];
        dist += diff0 * diff0;
        dist += diff1 * diff1;
        dist += diff2 * diff2;
        dist += diff3 * diff3;
    }
    
    // 处理剩余元素
    for (; i < d; i++) {
        float diff = x[i] - y[i];
        dist += diff * diff;
    }
    
    return dist;
}

// ===== 当前实现 (根据编译时选择) =====
float L2_distance(const float* x, const float* y, int d) {
#ifdef USE_FAISS_OPTIMIZATION
    return L2_distance_faiss(x, y, d);
#elif defined(__AVX2__)
    return L2_distance_avx2(x, y, d);
#else
    return L2_distance_unrolled(x, y, d);
#endif
}

// ===== 批量距离计算优化 (适用于top-k搜索) =====
void batch_l2_distance(const float* X, const float* y, float* distances, int n, int d) {
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < n; i++) {
        distances[i] = L2_distance(X + i * d, y, d);
    }
}

} // namespace dann
