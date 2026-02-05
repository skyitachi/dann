#include "dann/ivf_index.h"

namespace dann {

template <typename T = float>
IVFIndex<T>::IVFIndex(int dimension, int nlist, int nprobe, const std::string index_build_path)
    : dimension_(dimension), nlist_(nlist), nprobe_(nprobe), index_build_path_(index_build_path) {
}

template <typename T = float>
void build_index(std::vector<T>& vectors, const std::vector<int64_t>& ids) {
    // 采样向量的数量
}

}