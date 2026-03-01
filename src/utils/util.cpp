//
// Created by skyitachi on 2026/2/22.
//
#include "dann/utils.h"
#include "dann/types.h"

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
}