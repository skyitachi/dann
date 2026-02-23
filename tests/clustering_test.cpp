//
// Created by skyitachi on 2026/2/23.
//
#include <gtest/gtest.h>
#include "dann/clustering.h"

#include <random>

class ClusteringTest: public ::testing::Test {
protected:
  void SetUp() override {

  }
  void generate_test_data() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<float> dist(0.0f, 1.0f);

    const size_t num_vectors = 65536;
    test_vectors_.clear();
    test_ids_.clear();

    for (size_t i = 0; i < num_vectors; ++i) {
      std::vector<float> vector(64);
      for (int j = 0; j < 64; ++j) {
        vector[j] = dist(gen);
      }

      test_vectors_.insert(test_vectors_.end(), vector.begin(), vector.end());
      test_ids_.push_back(static_cast<int64_t>(i));
    }
  }

private:
  std::vector<float> test_vectors_;
  std::vector<int64_t> test_ids_;
};
