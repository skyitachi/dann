#include <iostream>
#include <vector>
#include <chrono>
#include <cassert>
#include <string>
#include <memory>

#include "dann/distributed_index_ivf.h"

#include <H5Cpp.h>

using namespace std::chrono;

static std::vector<float> read_hdf5_dataset(const std::string& hdf5_path, const std::string& dataset_name, int& out_rows, int& out_cols) {
    H5::Exception::dontPrint();
    H5::H5File file(hdf5_path, H5F_ACC_RDONLY);

    H5::DataSet dataset = file.openDataSet(dataset_name);
    H5::DataSpace space = dataset.getSpace();

    int ndims = space.getSimpleExtentNdims();
    if (ndims != 2) {
        throw std::runtime_error("Expected 2D dataset");
    }

    hsize_t dims[2];
    space.getSimpleExtentDims(dims, nullptr);

    out_rows = static_cast<int>(dims[0]);
    out_cols = static_cast<int>(dims[1]);

    std::vector<float> data(out_rows * out_cols);
    dataset.read(data.data(), H5::PredType::NATIVE_FLOAT, space);

    return data;
}

int main() {
    const std::string hdf5_path = "data/nytimes-256-angular.hdf5";
    const int test_samples = 10000;
    const int k = 10;
    const int shards = 4;
    const int dimension = 256;
    const int nlist = 4096;
    const int nprobe = 64;

    std::cout << "=== DANN DistributedIndexIVF Benchmark ===" << std::endl;
    std::cout << "Loading data from " << hdf5_path << std::endl;

    auto load_start = high_resolution_clock::now();

    int train_rows, train_cols, test_rows, test_cols;
    std::vector<float> train_data = read_hdf5_dataset(hdf5_path, "train", train_rows, train_cols);
    std::vector<float> test_data = read_hdf5_dataset(hdf5_path, "test", test_rows, test_cols);

    assert(train_cols == test_cols && train_cols == dimension);

    auto load_end = high_resolution_clock::now();
    auto load_duration = duration_cast<milliseconds>(load_end - load_start);

    std::cout << "Train data: " << train_rows << " x " << train_cols << std::endl;
    std::cout << "Test data: " << test_rows << " x " << test_cols << std::endl;
    std::cout << "Data loading time: " << load_duration.count() << " ms" << std::endl;

    std::vector<int64_t> train_ids(train_rows);
    for (int64_t i = 0; i < train_rows; ++i) {
        train_ids[i] = i;
    }

    auto build_start = high_resolution_clock::now();

    std::cout << "\nBuilding DistributedIndexIVF..." << std::endl;
    std::cout << "shards: " << shards << std::endl;
    std::cout << "nodes: 2 (node_0, node_1)" << std::endl;

    dann::DistributedIndexIVF index("dann_bench", dimension, shards, nlist, nprobe, {"node_0", "node_1"});

    std::cout << "Adding " << train_rows << " vectors to index..." << std::endl;
    index.add_vectors(train_data, train_ids);

    auto build_end = high_resolution_clock::now();
    auto build_duration = duration_cast<milliseconds>(build_end - build_start);

    std::cout << "Index building time: " << build_duration.count() << " ms" << std::endl;
    std::cout << "Dimension: " << index.dimension() << std::endl;
    std::cout << "Index type: " << index.index_type() << std::endl;

    auto search_start = high_resolution_clock::now();

    std::cout << "\nSearching for " << k << " nearest neighbors for " << test_samples << " queries..." << std::endl;

    int64_t total_results = 0;
    int64_t successful_queries = 0;

    for (int i = 0; i < test_samples; ++i) {
        std::vector<float> query(test_data.begin() + i * dimension, test_data.begin() + (i + 1) * dimension);
        auto results = index.search(query, k);

        if (results.size() > 0) {
            successful_queries++;
            total_results += results.size();
        }
    }

    auto search_end = high_resolution_clock::now();
    auto search_duration = duration_cast<milliseconds>(search_end - search_start);

    std::cout << "Search time: " << search_duration.count() << " ms" << std::endl;
    std::cout << "Average query latency: " << static_cast<double>(search_duration.count()) / test_samples << " ms" << std::endl;
    std::cout << "Successful queries: " << successful_queries << "/" << test_samples << std::endl;

    std::cout << "\nSample results (first query):" << std::endl;
    std::vector<float> first_query(test_data.begin(), test_data.begin() + dimension);
    auto first_results = index.search(first_query, k);
    for (size_t i = 0; i < std::min<size_t>(first_results.size(), 5); ++i) {
        std::cout << "  Result " << (i + 1) << ": ID=" << first_results[i].id << ", distance=" << first_results[i].distance << std::endl;
    }

    return 0;
}
