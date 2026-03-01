//
// Created by skyitachi on 2026/2/22.
//

#ifndef DANN_UTILS_H
#define DANN_UTILS_H
#include <cstdint>

namespace dann
{
    float L2_distance(const float* x, const float* y, int d);
    int64_t find_closest(const float *x, const float* y, int d, int n);

}
#endif //DANN_UTILS_H