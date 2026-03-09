//
// Created by skyitachi on 2026/3/9.
//
#include "dann/index_factory.h"
#include "dann/distributed_index_ivf.h"

namespace dann {

void IndexShardFactory::init(std::string name, std::string index_type, int dim,  int shards, int ef, int ef_construction, faiss::MetricType metric, std::vector<std::string> nodes) {
  if (index_type == "IVF") {
    index_map_[index_type] = std::make_shared<DistributedIndexIVF>(name, dim, shards, nodes);
  } else if (index_type == "HNSW") {

  }
}


}
