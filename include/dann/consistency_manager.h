#pragma once

#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <queue>
#include <thread>
#include <condition_variable>
#include "dann/types.h"

namespace dann {

class ConsistencyManager {
public:
    ConsistencyManager(const std::string& node_id);
    ~ConsistencyManager();
    
    // Eventual consistency operations
    bool propagate_operation(const IndexOperation& operation);
    bool apply_operation(const IndexOperation& operation);
    
    // Conflict resolution
    IndexOperation resolve_conflict(const std::vector<IndexOperation>& operations);
    bool has_conflict(const IndexOperation& op1, const IndexOperation& op2);
    
    // Version management
    uint64_t get_vector_version(int64_t vector_id) const;
    void update_vector_version(int64_t vector_id, uint64_t version);
    
    // Replication
    bool replicate_to_nodes(const IndexOperation& operation, const std::vector<std::string>& target_nodes);
    std::vector<IndexOperation> get_pending_replications();
    void mark_replication_complete(const std::string& operation_id);
    
    // Anti-entropy
    void start_anti_entropy();
    void stop_anti_entropy();
    bool sync_with_node(const std::string& node_id);
    
    // Vector clock support
    struct VectorClock {
        std::unordered_map<std::string, uint64_t> clock;
        
        void increment(const std::string& node_id);
        void update(const VectorClock& other);
        bool happens_before(const VectorClock& other) const;
        bool is_concurrent(const VectorClock& other) const;
    };
    
    VectorClock get_vector_clock(int64_t vector_id) const;
    void update_vector_clock(int64_t vector_id, const VectorClock& clock);
    
private:
    std::string node_id_;
    std::atomic<bool> running_;
    
    mutable std::mutex operations_mutex_;
    std::queue<IndexOperation> operation_queue_;
    std::unordered_map<std::string, IndexOperation> pending_operations_;
    
    mutable std::mutex versions_mutex_;
    std::unordered_map<int64_t, uint64_t> vector_versions_;
    std::unordered_map<int64_t, VectorClock> vector_clocks_;
    
    std::thread propagation_thread_;
    std::thread anti_entropy_thread_;
    std::condition_variable operation_cv_;
    
    // Operation propagation
    void propagation_loop();
    bool send_operation_to_node(const IndexOperation& operation, const std::string& node_id);
    
    // Anti-entropy
    void anti_entropy_loop();
    std::vector<IndexOperation> compute_differences(const std::string& node_id);
    
    // Conflict resolution strategies
    IndexOperation last_writer_wins(const std::vector<IndexOperation>& operations);
    IndexOperation merge_vectors(const std::vector<IndexOperation>& operations);
    
    // Utilities
    std::string generate_operation_id(const IndexOperation& operation);
    bool is_operation_applied(const std::string& operation_id);
};

} // namespace dann
