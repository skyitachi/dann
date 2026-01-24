#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <chrono>

namespace dann {

struct InternalSearchResult {
    int64_t id;
    float distance;
    std::vector<float> vector;
    
    InternalSearchResult(int64_t id = -1, float distance = 0.0f, const std::vector<float>& vector = {})
        : id(id), distance(distance), vector(vector) {}
};

struct InternalIndexOperation {
    enum Type { ADD, DELETE, UPDATE };
    
    Type type;
    int64_t id;
    std::vector<float> vector;
    uint64_t timestamp;
    uint64_t version;
    
    InternalIndexOperation() : type(ADD), id(0), vector(), timestamp(0), version(0) {}
    InternalIndexOperation(Type t, int64_t i, uint64_t ts, uint64_t ver)
        : type(t), id(i), vector(), timestamp(ts), version(ver) {}
    InternalIndexOperation(Type t, int64_t i, const std::vector<float>& v, uint64_t ts, uint64_t ver)
        : type(t), id(i), vector(v), timestamp(ts), version(ver) {}
};

struct InternalNodeInfo {
    std::string node_id;
    std::string address;
    int port;
    bool is_active;
    uint64_t last_heartbeat;
    std::vector<int> shard_ids;
    
    InternalNodeInfo() : node_id(""), address(""), port(0), is_active(false), last_heartbeat(0) {}
    InternalNodeInfo(const std::string& id, const std::string& addr, int p)
        : node_id(id), address(addr), port(p), is_active(false), last_heartbeat(0) {}
};

struct InternalQueryRequest {
    std::vector<float> query_vector;
    int k;
    std::string consistency_level;
    uint64_t timeout_ms;
    
    InternalQueryRequest(const std::vector<float>& vec, int k_val = 10)
        : query_vector(vec), k(k_val), consistency_level("eventual"), timeout_ms(5000) {}
};

struct InternalQueryResponse {
    bool success;
    std::string error_message;
    std::vector<InternalSearchResult> results;
    uint64_t query_time_ms;
    
    InternalQueryResponse(bool succ = true, const std::string& err = "")
        : success(succ), error_message(err), query_time_ms(0) {}
};

struct InternalBulkLoadRequest {
    std::vector<float> vectors;
    std::vector<int64_t> ids;
    int batch_size;
    bool overwrite_existing;
    
    InternalBulkLoadRequest(const std::vector<float>& vecs, const std::vector<int64_t>& ids_vec, int batch = 1000)
        : vectors(vecs), ids(ids_vec), batch_size(batch), overwrite_existing(false) {}
};

} // namespace dann
