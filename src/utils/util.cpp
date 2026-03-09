//
// Created by skyitachi on 2026/2/22.
//
#include "dann/utils.h"

#include <algorithm>
#include <queue>

namespace dann
{

float L2_distance(const float* x, const float* y, int d) {
    float dist = 0.0f;
    for (int i = 0; i < d; i++) {
        float diff = x[i] - y[i];
        dist += diff * diff;
    }
    return dist;
}

int64_t find_closest(const float *x, const float* y, int d, int n) {
    float min_dis = std::numeric_limits<float>::max();
    int r = 0;
    for (int i = 0; i < n; i++) {
        float dis = L2_distance(x + i * d, y, d);
        if (dis < min_dis) {
            min_dis = dis;
            r = i;
        }
    }
    return r;
}

std::vector<int64_t> find_closest_k(const float *x, const float *y, int d, int n, int k) {
    DistanceWithIndexQueue queue;
    for (int i = 0; i < n; i++) {
        float dis = L2_distance(x + i * d, y, d);
        if (queue.size() < k) {
            // 队列未满，直接添加元素
            queue.emplace(dis, i);
        } else {
            // 队列已满，比较当前距离与队列中最大距离
            if (dis < queue.top().distance) {
                // 当前距离更小，替换最大的元素
                queue.pop();
                queue.emplace(dis, i);
            }
        }
    }
    
    // 提取结果并按距离排序
    std::vector<int64_t> result;
    result.reserve(queue.size());
    while (!queue.empty()) {
        result.push_back(queue.top().index);
        queue.pop();
    }
    
    // 由于是最大堆，需要反转结果以得到按距离从近到远的顺序
    std::reverse(result.begin(), result.end());
    return result;
}

std::vector<DistanceWithIndex> find_closest_k_with_distance(const float *x, const float *y, int d, int n, int k) {
    DistanceWithIndexQueue queue;
    for (int i = 0; i < n; i++) {
        float dis = L2_distance(x + i * d, y, d);
        if (queue.size() < k) {
            // 队列未满，直接添加元素
            queue.emplace(dis, i);
        } else {
            // 队列已满，比较当前距离与队列中最大距离
            if (dis < queue.top().distance) {
                // 当前距离更小，替换最大的元素
                queue.pop();
                queue.emplace(dis, i);
            }
        }
    }

    // 提取结果并按距离排序
    std::vector<DistanceWithIndex> result;
    result.reserve(queue.size());
    while (!queue.empty()) {
        result.push_back(queue.top());
        queue.pop();
    }
    std::reverse(result.begin(), result.end());
    return std::move(result);
}

std::vector<DistanceWithIndex> find_closest_k_with_distance(const std::vector<float>& x, const std::vector<float>& y, int d, int n, int k) {
    return find_closest_k_with_distance(&x[0], &y[0], d, n, k);
}

std::vector<int64_t> find_closest_k(const std::vector<float>& x, const std::vector<float> &y, int d, int n, int k) {
    return find_closest_k(x.data(), y.data(), d, n, k);
}
}