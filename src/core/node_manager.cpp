#include "dann/node_manager.h"
#include <algorithm>
#include <chrono>

namespace dann {

NodeManager::NodeManager(const std::string& node_id, const std::string& address, int port)
    : node_id_(node_id),
      address_(address),
      port_(port),
      running_(false),
      health_monitor_running_(false) {}

NodeManager::~NodeManager() {
    stop();
}

bool NodeManager::start() {
    running_ = true;
    NodeInfo self(node_id_, address_, port_);
    self.is_active = true;
    self.last_heartbeat = std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::system_clock::now().time_since_epoch())
                              .count();
    register_node(self);
    return true;
}

bool NodeManager::stop() {
    running_ = false;
    stop_health_monitor();
    return true;
}

bool NodeManager::is_running() const {
    return running_.load();
}

bool NodeManager::join_cluster(const std::vector<std::string>& /*seed_nodes*/) {
    return true;
}

bool NodeManager::leave_cluster() {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    cluster_nodes_.clear();
    return true;
}

std::vector<NodeInfo> NodeManager::get_cluster_nodes() const {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    std::vector<NodeInfo> nodes;
    nodes.reserve(cluster_nodes_.size());
    for (const auto& entry : cluster_nodes_) {
        nodes.push_back(entry.second);
    }
    return nodes;
}

void NodeManager::register_node(const NodeInfo& node) {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    cluster_nodes_[node.node_id] = node;
}

void NodeManager::unregister_node(const std::string& node_id) {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    cluster_nodes_.erase(node_id);
}

void NodeManager::update_heartbeat(const std::string& node_id) {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    auto it = cluster_nodes_.find(node_id);
    if (it != cluster_nodes_.end()) {
        it->second.last_heartbeat = std::chrono::duration_cast<std::chrono::milliseconds>(
                                        std::chrono::system_clock::now().time_since_epoch())
                                        .count();
        it->second.is_active = true;
    }
}

std::vector<int> NodeManager::get_assigned_shards() const {
    std::lock_guard<std::mutex> lock(shards_mutex_);
    return assigned_shards_;
}

void NodeManager::assign_shards(const std::vector<int>& shard_ids) {
    std::lock_guard<std::mutex> lock(shards_mutex_);
    assigned_shards_ = shard_ids;
    shard_to_node_.clear();
    for (int shard_id : shard_ids) {
        shard_to_node_[shard_id] = node_id_;
    }
}

std::string NodeManager::get_node_for_shard(int shard_id) const {
    std::lock_guard<std::mutex> lock(shards_mutex_);
    auto it = shard_to_node_.find(shard_id);
    if (it != shard_to_node_.end()) {
        return it->second;
    }
    return {};
}

void NodeManager::start_health_monitor() {
    if (health_monitor_running_) {
        return;
    }
    health_monitor_running_ = true;
    health_monitor_thread_ = std::thread(&NodeManager::health_monitor_loop, this);
}

void NodeManager::stop_health_monitor() {
    if (!health_monitor_running_) {
        return;
    }
    health_monitor_running_ = false;
    if (health_monitor_thread_.joinable()) {
        health_monitor_thread_.join();
    }
}

std::vector<NodeInfo> NodeManager::get_failed_nodes() const {
    std::vector<NodeInfo> failed;
    const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    for (const auto& entry : cluster_nodes_) {
        if (entry.second.is_active && now_ms - entry.second.last_heartbeat > 30000) {
            failed.push_back(entry.second);
        }
    }
    return failed;
}

void NodeManager::set_node_join_callback(NodeEventCallback callback) {
    node_join_callback_ = std::move(callback);
}

void NodeManager::set_node_leave_callback(NodeEventCallback callback) {
    node_leave_callback_ = std::move(callback);
}

void NodeManager::health_monitor_loop() {
    while (health_monitor_running_) {
        check_node_health();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void NodeManager::check_node_health() {
    auto failed = get_failed_nodes();
    for (const auto& node : failed) {
        handle_node_failure(node.node_id);
    }
}

void NodeManager::handle_node_failure(const std::string& node_id) {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    auto it = cluster_nodes_.find(node_id);
    if (it != cluster_nodes_.end()) {
        it->second.is_active = false;
        if (node_leave_callback_) {
            node_leave_callback_(it->second);
        }
    }
}

void NodeManager::gossip_loop() {}

void NodeManager::send_heartbeat() {}

void NodeManager::broadcast_node_info() {}

int NodeManager::get_shard_for_vector(const std::vector<float>& /*vector*/) const {
    return 0;
}

std::vector<std::string> NodeManager::get_replica_nodes(int /*shard_id*/) const {
    return {};
}

} // namespace dann
