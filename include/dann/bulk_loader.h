#pragma once

#include <vector>
#include <memory>
#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <future>
#include "dann/types.h"

namespace dann {

// Forward declarations
class VectorIndex;
class ConsistencyManager;

class BulkLoader {
public:
    BulkLoader(std::shared_ptr<VectorIndex> index, 
               std::shared_ptr<ConsistencyManager> consistency_manager);
    ~BulkLoader();
    
    // Bulk loading operations
    std::future<bool> load_vectors(const BulkLoadRequest& request);
    bool load_vectors_sync(const BulkLoadRequest& request);
    
    // Progress tracking
    struct LoadProgress {
        uint64_t total_vectors;
        uint64_t processed_vectors;
        uint64_t failed_vectors;
        double progress_percentage;
        std::string status;
        uint64_t start_time_ms;
        uint64_t estimated_completion_ms;
    };
    
    LoadProgress get_progress(const std::string& load_id) const;
    std::vector<std::string> get_active_loads() const;
    
    // Configuration
    void set_batch_size(size_t batch_size);
    void set_max_concurrent_loads(size_t max_loads);
    void set_retry_attempts(size_t retry_attempts);
    
    // Distributed bulk loading
    std::future<bool> distributed_load(const BulkLoadRequest& request, 
                                       const std::vector<std::string>& target_nodes);
    bool coordinate_distributed_load(const BulkLoadRequest& request);
    
    // Validation and preprocessing
    bool validate_vectors(const std::vector<float>& vectors, const std::vector<int64_t>& ids);
    std::vector<float> normalize_vectors(const std::vector<float>& vectors);
    std::vector<int64_t> deduplicate_ids(const std::vector<int64_t>& ids, 
                                         const std::vector<float>& vectors);
    
    // Index optimization
    bool optimize_index_after_load();
    bool rebuild_index(const std::string& optimization_strategy);
    
    // Error handling and recovery
    void set_error_handling_strategy(const std::string& strategy);
    bool resume_failed_load(const std::string& load_id);
    void cancel_load(const std::string& load_id);
    
    // Metrics
    struct LoadMetrics {
        uint64_t total_loads;
        uint64_t successful_loads;
        uint64_t failed_loads;
        double avg_load_time_ms;
        uint64_t total_vectors_loaded;
        double avg_vectors_per_second;
    };
    
    LoadMetrics get_metrics() const;
    void reset_metrics();
    
private:
    std::shared_ptr<VectorIndex> index_;
    std::shared_ptr<ConsistencyManager> consistency_manager_;
    
    // Configuration
    size_t batch_size_;
    size_t max_concurrent_loads_;
    size_t retry_attempts_;
    std::string error_handling_strategy_;
    
    // Load tracking
    struct LoadTask {
        std::string load_id;
        BulkLoadRequest request;
        LoadProgress progress;
        std::promise<bool> promise;
        std::atomic<bool> cancelled;
        
        LoadTask(const std::string& id, const BulkLoadRequest& req)
            : load_id(id), request(req), cancelled(false) {
            progress = LoadProgress{};
            progress.total_vectors = req.ids.size();
            progress.status = "pending";
            progress.start_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
        }
    };
    
    mutable std::mutex tasks_mutex_;
    std::unordered_map<std::string, std::unique_ptr<LoadTask>> active_tasks_;
    std::queue<std::string> task_queue_;
    
    // Worker threads
    std::vector<std::thread> worker_threads_;
    std::atomic<bool> running_;
    std::condition_variable queue_cv_;
    
    // Metrics
    mutable std::mutex metrics_mutex_;
    LoadMetrics metrics_;
    
    // Core loading logic
    void worker_loop();
    bool process_load_task(LoadTask& task);
    bool load_batch(const std::vector<float>& vectors, 
                   const std::vector<int64_t>& ids,
                   LoadTask& task);
    
    // Distributed loading
    bool distribute_load_to_nodes(const BulkLoadRequest& request, 
                                 const std::vector<std::string>& nodes);
    std::vector<BulkLoadRequest> partition_request(const BulkLoadRequest& request, 
                                                  const std::vector<std::string>& nodes);
    
    // Index optimization
    bool train_index_if_needed(const std::vector<float>& sample_vectors);
    bool update_index_parameters();
    
    // Error handling
    bool handle_load_error(LoadTask& task, const std::string& error);
    bool retry_failed_batch(LoadTask& task, size_t retry_count);
    
    // Utilities
    std::string generate_load_id();
    void update_progress(LoadTask& task, uint64_t processed, uint64_t failed);
    void complete_task(LoadTask& task, bool success);
    
    // Worker pool management
    void start_worker_pool();
    void stop_worker_pool();
};

} // namespace dann
