#include "dann/consistency_manager.h"
#include <chrono>

namespace dann {

ConsistencyManager::ConsistencyManager(const std::string& node_id)
    : node_id_(node_id),
      running_(false) {}

ConsistencyManager::~ConsistencyManager() {
    stop_anti_entropy();
}

bool ConsistencyManager::propagate_operation(const IndexOperation& operation) {
    std::lock_guard<std::mutex> lock(operations_mutex_);
    operation_queue_.push(operation);
    pending_operations_[generate_operation_id(operation)] = operation;
    operation_cv_.notify_all();
    return true;
}

bool ConsistencyManager::apply_operation(const IndexOperation& operation) {
    update_vector_version(operation.id, operation.version);
    return true;
}

IndexOperation ConsistencyManager::resolve_conflict(const std::vector<IndexOperation>& operations) {
    return last_writer_wins(operations);
}

bool ConsistencyManager::has_conflict(const IndexOperation& op1, const IndexOperation& op2) {
    return op1.id == op2.id && op1.version != op2.version;
}

uint64_t ConsistencyManager::get_vector_version(int64_t vector_id) const {
    std::lock_guard<std::mutex> lock(versions_mutex_);
    auto it = vector_versions_.find(vector_id);
    if (it == vector_versions_.end()) {
        return 0;
    }
    return it->second;
}

void ConsistencyManager::update_vector_version(int64_t vector_id, uint64_t version) {
    std::lock_guard<std::mutex> lock(versions_mutex_);
    vector_versions_[vector_id] = version;
}

bool ConsistencyManager::replicate_to_nodes(const IndexOperation& /*operation*/,
                                            const std::vector<std::string>& /*target_nodes*/) {
    return true;
}

std::vector<IndexOperation> ConsistencyManager::get_pending_replications() {
    std::lock_guard<std::mutex> lock(operations_mutex_);
    std::vector<IndexOperation> ops;
    ops.reserve(pending_operations_.size());
    for (const auto& entry : pending_operations_) {
        ops.push_back(entry.second);
    }
    return ops;
}

void ConsistencyManager::mark_replication_complete(const std::string& operation_id) {
    std::lock_guard<std::mutex> lock(operations_mutex_);
    pending_operations_.erase(operation_id);
}

void ConsistencyManager::start_anti_entropy() {
    if (running_) {
        return;
    }
    running_ = true;
    anti_entropy_thread_ = std::thread(&ConsistencyManager::anti_entropy_loop, this);
}

void ConsistencyManager::stop_anti_entropy() {
    if (!running_) {
        return;
    }
    running_ = false;
    operation_cv_.notify_all();
    if (anti_entropy_thread_.joinable()) {
        anti_entropy_thread_.join();
    }
}

bool ConsistencyManager::sync_with_node(const std::string& /*node_id*/) {
    return true;
}

void ConsistencyManager::VectorClock::increment(const std::string& node_id) {
    ++clock[node_id];
}

void ConsistencyManager::VectorClock::update(const VectorClock& other) {
    for (const auto& entry : other.clock) {
        auto it = clock.find(entry.first);
        if (it == clock.end() || it->second < entry.second) {
            clock[entry.first] = entry.second;
        }
    }
}

bool ConsistencyManager::VectorClock::happens_before(const VectorClock& other) const {
    bool strictly_less = false;
    for (const auto& entry : clock) {
        auto it = other.clock.find(entry.first);
        const uint64_t other_val = it == other.clock.end() ? 0 : it->second;
        if (entry.second > other_val) {
            return false;
        }
        if (entry.second < other_val) {
            strictly_less = true;
        }
    }
    return strictly_less;
}

bool ConsistencyManager::VectorClock::is_concurrent(const VectorClock& other) const {
    return !happens_before(other) && !other.happens_before(*this);
}

ConsistencyManager::VectorClock ConsistencyManager::get_vector_clock(int64_t vector_id) const {
    std::lock_guard<std::mutex> lock(versions_mutex_);
    auto it = vector_clocks_.find(vector_id);
    if (it == vector_clocks_.end()) {
        return {};
    }
    return it->second;
}

void ConsistencyManager::update_vector_clock(int64_t vector_id, const VectorClock& clock) {
    std::lock_guard<std::mutex> lock(versions_mutex_);
    vector_clocks_[vector_id] = clock;
}

void ConsistencyManager::propagation_loop() {
    while (running_) {
        std::unique_lock<std::mutex> lock(operations_mutex_);
        operation_cv_.wait_for(lock, std::chrono::milliseconds(100));
    }
}

bool ConsistencyManager::send_operation_to_node(const IndexOperation& /*operation*/,
                                                const std::string& /*node_id*/) {
    return true;
}

void ConsistencyManager::anti_entropy_loop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

std::vector<IndexOperation> ConsistencyManager::compute_differences(const std::string& /*node_id*/) {
    return {};
}

IndexOperation ConsistencyManager::last_writer_wins(const std::vector<IndexOperation>& operations) {
    if (operations.empty()) {
        return IndexOperation(IndexOperation::ADD, -1, {}, 0, 0);
    }
    const IndexOperation* latest = &operations.front();
    for (const auto& op : operations) {
        if (op.version > latest->version) {
            latest = &op;
        }
    }
    return *latest;
}

IndexOperation ConsistencyManager::merge_vectors(const std::vector<IndexOperation>& operations) {
    return last_writer_wins(operations);
}

std::string ConsistencyManager::generate_operation_id(const IndexOperation& operation) {
    return node_id_ + ":" + std::to_string(operation.id) + ":" + std::to_string(operation.version);
}

bool ConsistencyManager::is_operation_applied(const std::string& operation_id) {
    std::lock_guard<std::mutex> lock(operations_mutex_);
    return pending_operations_.find(operation_id) == pending_operations_.end();
}

} // namespace dann
