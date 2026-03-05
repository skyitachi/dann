//
// Created by skyitachi on 2026/2/22.
//

#ifndef DANN_UTILS_H
#define DANN_UTILS_H
#include "dann/types.h"

#include <cstdint>
#include <vector>

namespace dann {
  float L2_distance(const float* x, const float* y, int d);
  int64_t find_closest(const float *x, const float* y, int d, int n);
  std::vector<int64_t> find_closest_k(const float *x, const float *y, int d, int n, int k);
  std::vector<int64_t> find_closest_k(const std::vector<float>& x, const std::vector<float> &y, int d, int n, int k);

  std::vector<DistanceWithIndex> find_closest_k_with_distance(const float* x, const float* y, int d, int n, int k);
  std::vector<DistanceWithIndex> find_closest_k_with_distance(const std::vector<float>& x, const std::vector<float>& y, int d, int n, int k);

}
#endif //DANN_UTILS_H