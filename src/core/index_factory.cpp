//
// Created by skyitachi on 2026/3/9.
//
#include "dann/index_factory.h"
#include "dann/distributed_index_ivf.h"
#include "dann/vector_index.h"

namespace dann {

std::shared_ptr<IndexShard> IndexShardFactory::create(std::string name, std::string index_type, int dim,  int shards,
  int ef, int ef_construction, faiss::MetricType metric, std::vector<std::string> nodes) {
  (void)metric;
  if (index_type == "IVF") {
    return std::make_shared<DistributedIndexIVF>(name, dim, shards, nodes);
  } else if (index_type == "HNSW") {
    return std::make_shared<VectorIndex>(dim, index_type, ef, ef_construction);
  }
  return nullptr;
}


}
