//
// Created by skyitachi on 2026/2/22.
//
#include "dann/utils.h"

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
}