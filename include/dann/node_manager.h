#pragma once

#include <unordered_map>
#include <vector>
#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include "dann/types.h"

namespace dann {

class NodeManager {
public:
    NodeManager(const std::string& node_id, const std::string& address, int port);
    ~NodeManager();
    
    // Node lifecycle
    bool start();
    bool stop();
    bool is_running() const;
    
    // Cluster management
    bool join_cluster(const std::vector<std::string>& seed_nodes);
    bool leave_cluster();
    std::vector<NodeInfo> get_cluster_nodes() const;
    
    // Node discovery
    void register_node(const NodeInfo& node);
    void unregister_node(const std::string& node_id);
    void update_heartbeat(const std::string& node_id);
    
    // Shard management
    std::vector<int> get_assigned_shards() const;
    void assign_shards(const std::vector<int>& shard_ids);
    std::string get_node_for_shard(int shard_id) const;
    
    // Health monitoring
    void start_health_monitor();
    void stop_health_monitor();
    std::vector<NodeInfo> get_failed_nodes() const;
    
    // Event callbacks
    using NodeEventCallback = std::function<void(const NodeInfo&)>;
    void set_node_join_callback(NodeEventCallback callback);
    void set_node_leave_callback(NodeEventCallback callback);
    
private:
    std::string node_id_;
    std::string address_;
    int port_;
    std::atomic<bool> running_;
    
    mutable std::mutex nodes_mutex_;
    std::unordered_map<std::string, NodeInfo> cluster_nodes_;
    
    mutable std::mutex shards_mutex_;
    std::vector<int> assigned_shards_;
    std::unordered_map<int, std::string> shard_to_node_;
    
    std::thread health_monitor_thread_;
    std::atomic<bool> health_monitor_running_;
    
    NodeEventCallback node_join_callback_;
    NodeEventCallback node_leave_callback_;
    
    // Health monitoring
    void health_monitor_loop();
    void check_node_health();
    void handle_node_failure(const std::string& node_id);
    
    // Gossip protocol
    void gossip_loop();
    void send_heartbeat();
    void broadcast_node_info();
    
    // Consistent hashing
    int get_shard_for_vector(const std::vector<float>& vector) const;
    std::vector<std::string> get_replica_nodes(int shard_id) const;
};

} // namespace dann
