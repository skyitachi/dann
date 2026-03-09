#include <iostream>
#include <vector>
#include <chrono>
#include <cassert>
#include <string>
#include <memory>

#include <faiss/Index.h>
#include <faiss/IndexFlat.h>
#include <faiss/IndexIVFFlat.h>
#include <faiss/impl/AuxIndexStructures.h>

#include <H5Cpp.h>

using namespace std::chrono;

static H5::DataSet open_first_existing_dataset(H5::H5File& file, const std::string& name) {
    return file.openDataSet(name);
}

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
    const int train_samples = 290000;
    const int test_samples = 10000;
    const int k = 10;
    const int nlist = 4096;
    const int nprobe = 64;

    std::cout << "=== Faiss IndexIVFFlat Benchmark ===" << std::endl;
    std::cout << "Loading data from " << hdf5_path << std::endl;

    auto load_start = high_resolution_clock::now();

    int train_rows, train_cols, test_rows, test_cols;
    std::vector<float> train_data = read_hdf5_dataset(hdf5_path, "train", train_rows, train_cols);
    std::vector<float> test_data = read_hdf5_dataset(hdf5_path, "test", test_rows, test_cols);

    assert(train_cols == test_cols);
    const int dimension = train_cols;
    std::cout << "Bench data dimension: " << dimension << std::endl;

    auto load_end = high_resolution_clock::now();
    auto load_duration = duration_cast<milliseconds>(load_end - load_start);

    std::cout << "Train data: " << train_rows << " x " << train_cols << std::endl;
    std::cout << "Test data: " << test_rows << " x " << test_cols << std::endl;
    std::cout << "Data loading time: " << load_duration.count() << " ms" << std::endl;

    auto build_start = high_resolution_clock::now();

    std::cout << "\nBuilding IndexIVFFlat..." << std::endl;
    std::cout << "nlist: " << nlist << std::endl;

    faiss::IndexIVFFlat* index = nullptr;

    auto quantizer = std::unique_ptr<faiss::IndexFlat>(new faiss::IndexFlat(dimension, faiss::METRIC_L2));
    
    // Enable verbose output for clustering
    faiss::ClusteringParameters cp;
    cp.verbose = true;
    cp.niter = 25;
    
    index = new faiss::IndexIVFFlat(quantizer.release(), dimension, nlist, faiss::METRIC_L2);
    index->cp = cp;

    const int n_train_samples = std::min(train_rows, train_samples);
    std::cout << "Training with " << n_train_samples << " vectors..." << std::endl;
    index->train(n_train_samples, train_data.data());

    std::cout << "Adding " << train_rows << " vectors to index..." << std::endl;
    index->add(train_rows, train_data.data());

    index->nprobe = nprobe;
    std::cout << "nprobe: " << nprobe << std::endl;

    auto build_end = high_resolution_clock::now();
    auto build_duration = duration_cast<milliseconds>(build_end - build_start);

    std::cout << "Index building time: " << build_duration.count() << " ms" << std::endl;
    std::cout << "Total vectors in index: " << index->ntotal << std::endl;

    auto search_start = high_resolution_clock::now();

    std::vector<float> distances(test_samples * k);
    std::vector<int64_t> labels(test_samples * k);

    std::cout << "\nSearching for " << k << " nearest neighbors for " << test_samples << " queries..." << std::endl;
    index->search(test_samples, test_data.data(), k, distances.data(), labels.data());

    auto search_end = high_resolution_clock::now();
    auto search_duration = duration_cast<milliseconds>(search_end - search_start);

    std::cout << "Search time: " << search_duration.count() << " ms" << std::endl;
    std::cout << "Average query latency: " << static_cast<double>(search_duration.count()) / test_samples << " ms" << std::endl;

    std::cout << "\nSample results (first query):" << std::endl;
    for (int i = 0; i < std::min(k, 5); ++i) {
        std::cout << "  Result " << (i + 1) << ": ID=" << labels[i] << ", distance=" << distances[i] << std::endl;
    }

    delete index;

    return 0;
}
