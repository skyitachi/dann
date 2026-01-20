#pragma once

#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <future>
#include <thread>
#include <atomic>
#include <mutex>
#include "dann/types.h"

namespace dann {

class QueryRouter {
public:
    QueryRouter(std::shared_ptr<NodeManager> node_manager);
    ~QueryRouter();
    
    // Query routing
    std::future<QueryResponse> route_query(const QueryRequest& request);
    QueryResponse execute_query(const QueryRequest& request);
    
    // Distributed query execution
    std::vector<QueryResponse> parallel_query(const QueryRequest& request, 
                                              const std::vector<std::string>& target_nodes);
    QueryResponse merge_results(const std::vector<QueryResponse>& responses);
    
    // Load balancing
    void set_load_balance_strategy(const std::string& strategy);
    std::string select_node(const std::vector<std::string>& candidates);
    
    // Query optimization
    QueryResponse optimized_search(const QueryRequest& request);
    std::vector<std::string> select_relevant_nodes(const std::vector<float>& query_vector);
    
    // Caching
    void enable_caching(bool enable);
    QueryResponse get_cached_result(const std::vector<float>& query_vector, int k);
    void cache_result(const std::vector<float>& query_vector, int k, const QueryResponse& response);
    
    // Fallback handling
    QueryResponse handle_node_failure(const QueryRequest& request, const std::string& failed_node);
    bool is_node_available(const std::string& node_id);
    
    // Metrics and monitoring
    struct QueryMetrics {
        uint64_t total_queries;
        uint64_t successful_queries;
        uint64_t failed_queries;
        double avg_response_time_ms;
        std::unordered_map<std::string, uint64_t> node_query_counts;
        std::unordered_map<std::string, double> node_response_times;
    };
    
    QueryMetrics get_metrics() const;
    void reset_metrics();
    
private:
    std::shared_ptr<NodeManager> node_manager_;
    std::string load_balance_strategy_;
    std::atomic<bool> caching_enabled_;
    
    mutable std::mutex metrics_mutex_;
    QueryMetrics metrics_;
    
    mutable std::mutex cache_mutex_;
    std::unordered_map<std::string, QueryResponse> query_cache_;
    
    std::vector<std::thread> worker_threads_;
    std::atomic<bool> running_;
    std::queue<std::function<void()>> task_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    
    // Load balancing strategies
    std::string round_robin_strategy(const std::vector<std::string>& candidates);
    std::string least_loaded_strategy(const std::vector<std::string>& candidates);
    std::string hash_based_strategy(const std::vector<std::string>& candidates, 
                                   const std::vector<float>& query_vector);
    
    // Result merging
    std::vector<SearchResult> merge_search_results(const std::vector<std::vector<SearchResult>>& result_sets);
    std::vector<SearchResult> deduplicate_results(const std::vector<SearchResult>& results);
    std::vector<SearchResult> rank_results(const std::vector<SearchResult>& results);
    
    // Caching utilities
    std::string generate_cache_key(const std::vector<float>& query_vector, int k);
    void cleanup_cache();
    
    // Worker thread pool
    void worker_loop();
    void start_worker_pool(int num_workers = 4);
    void stop_worker_pool();
    
    // Communication with other nodes
    QueryResponse send_query_to_node(const QueryRequest& request, const std::string& node_id);
    bool send_heartbeat_to_node(const std::string& node_id);
};

} // namespace dann
