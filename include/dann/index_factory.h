//
// Created by skyitachi on 2026/3/9.
//

#ifndef DANN_INDEX_FACTORY_H
#define DANN_INDEX_FACTORY_H
#include <map>
#include <memory>

#include "dann/types.h"
#include "faiss/impl/AuxIndexStructures.h"

namespace dann {
class IndexShard;
class IndexShardFactory {

public:
  std::shared_ptr<IndexShard> create(std::string name, std::string index_type, int dim, int shards, int ef, int ef_construction,
    faiss::MetricType metric = faiss::METRIC_L2, std::vector<std::string> nodes_ = {});
  std::shared_ptr<IndexShard> get_index(std::string index_type);

private:
  std::map<std::string, std::shared_ptr<IndexShard>> index_map_;

};
inline IndexShardFactory index_shard_factory;
inline IndexShardFactory* get_global_index_factory() {
  return &index_shard_factory;
}
}
#endif // DANN_INDEX_FACTORY_H
