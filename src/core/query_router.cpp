#include "dann/query_router.h"
#include <chrono>

namespace dann {

QueryRouter::QueryRouter(std::shared_ptr<NodeManager> node_manager)
    : node_manager_(std::move(node_manager)),
      load_balance_strategy_("round_robin"),
      caching_enabled_(false),
      running_(false) {
    reset_metrics();
}

QueryRouter::~QueryRouter() {
    stop_worker_pool();
}

std::future<QueryResponse> QueryRouter::route_query(const QueryRequest& request) {
    return std::async(std::launch::async, [this, request]() { return execute_query(request); });
}

QueryResponse QueryRouter::execute_query(const QueryRequest& request) {
    const auto start = std::chrono::steady_clock::now();

    if (caching_enabled_) {
        auto cached = get_cached_result(request.query_vector, request.k);
        if (cached.success || !cached.error_message.empty()) {
            return cached;
        }
    }

    QueryResponse response(true);
    response.results.reserve(static_cast<size_t>(request.k));

    const auto end = std::chrono::steady_clock::now();
    response.query_time_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        metrics_.total_queries++;
        metrics_.successful_queries++;
        metrics_.avg_response_time_ms =
            (metrics_.avg_response_time_ms * (metrics_.total_queries - 1) + response.query_time_ms) /
            metrics_.total_queries;
    }

    if (caching_enabled_) {
        cache_result(request.query_vector, request.k, response);
    }

    return response;
}

std::vector<QueryResponse> QueryRouter::parallel_query(const QueryRequest& request,
                                                       const std::vector<std::string>& target_nodes) {
    std::vector<QueryResponse> responses;
    responses.reserve(target_nodes.size());
    for (const auto& node : target_nodes) {
        responses.push_back(send_query_to_node(request, node));
    }
    return responses;
}

QueryResponse QueryRouter::merge_results(const std::vector<QueryResponse>& responses) {
    QueryResponse merged(true);
    for (const auto& response : responses) {
        merged.results.insert(merged.results.end(), response.results.begin(), response.results.end());
        if (!response.success) {
            merged.success = false;
            merged.error_message = response.error_message;
        }
    }
    return merged;
}

void QueryRouter::set_load_balance_strategy(const std::string& strategy) {
    load_balance_strategy_ = strategy;
}

std::string QueryRouter::select_node(const std::vector<std::string>& candidates) {
    if (candidates.empty()) {
        return {};
    }
    return round_robin_strategy(candidates);
}

QueryResponse QueryRouter::optimized_search(const QueryRequest& request) {
    return execute_query(request);
}

std::vector<std::string> QueryRouter::select_relevant_nodes(const std::vector<float>& /*query_vector*/) {
    return {};
}

void QueryRouter::enable_caching(bool enable) {
    caching_enabled_ = enable;
}

QueryResponse QueryRouter::get_cached_result(const std::vector<float>& query_vector, int k) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    auto key = generate_cache_key(query_vector, k);
    auto it = query_cache_.find(key);
    if (it != query_cache_.end()) {
        return it->second;
    }
    return QueryResponse(false, "");
}

void QueryRouter::cache_result(const std::vector<float>& query_vector, int k, const QueryResponse& response) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    query_cache_[generate_cache_key(query_vector, k)] = response;
}

QueryResponse QueryRouter::handle_node_failure(const QueryRequest& request, const std::string& /*failed_node*/) {
    return execute_query(request);
}

bool QueryRouter::is_node_available(const std::string& /*node_id*/) {
    return true;
}

QueryRouter::QueryMetrics QueryRouter::get_metrics() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    return metrics_;
}

void QueryRouter::reset_metrics() {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    metrics_ = QueryMetrics{};
    metrics_.total_queries = 0;
    metrics_.successful_queries = 0;
    metrics_.failed_queries = 0;
    metrics_.avg_response_time_ms = 0.0;
}

std::string QueryRouter::round_robin_strategy(const std::vector<std::string>& candidates) {
    static std::atomic<size_t> counter{0};
    const size_t idx = counter++ % candidates.size();
    return candidates[idx];
}

std::string QueryRouter::least_loaded_strategy(const std::vector<std::string>& candidates) {
    if (candidates.empty()) {
        return {};
    }
    return candidates.front();
}

std::string QueryRouter::hash_based_strategy(const std::vector<std::string>& candidates,
                                             const std::vector<float>& /*query_vector*/) {
    if (candidates.empty()) {
        return {};
    }
    return candidates.front();
}

std::vector<SearchResult> QueryRouter::merge_search_results(
    const std::vector<std::vector<SearchResult>>& result_sets) {
    std::vector<SearchResult> merged;
    for (const auto& set : result_sets) {
        merged.insert(merged.end(), set.begin(), set.end());
    }
    return merged;
}

std::vector<SearchResult> QueryRouter::deduplicate_results(const std::vector<SearchResult>& results) {
    return results;
}

std::vector<SearchResult> QueryRouter::rank_results(const std::vector<SearchResult>& results) {
    return results;
}

std::string QueryRouter::generate_cache_key(const std::vector<float>& query_vector, int k) {
    std::string key = std::to_string(k) + ":";
    key.reserve(key.size() + query_vector.size() * 6);
    for (float value : query_vector) {
        key.append(std::to_string(value));
        key.push_back(',');
    }
    return key;
}

void QueryRouter::cleanup_cache() {}

void QueryRouter::worker_loop() {}

void QueryRouter::start_worker_pool(int /*num_workers*/) {
    running_ = true;
}

void QueryRouter::stop_worker_pool() {
    running_ = false;
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    worker_threads_.clear();
}

QueryResponse QueryRouter::send_query_to_node(const QueryRequest& request, const std::string& /*node_id*/) {
    return execute_query(request);
}

bool QueryRouter::send_heartbeat_to_node(const std::string& /*node_id*/) {
    return true;
}

} // namespace dann
