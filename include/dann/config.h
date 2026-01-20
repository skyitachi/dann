#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include "dann/types.h"

namespace dann {

class Config {
public:
    static Config& instance();
    
    // Loading configuration
    bool load_from_file(const std::string& filename);
    bool load_from_string(const std::string& json_string);
    bool load_from_env();
    
    // Saving configuration
    bool save_to_file(const std::string& filename);
    std::string to_string() const;
    
    // Node configuration
    struct NodeConfig {
        std::string id;
        std::string address;
        int port;
        std::vector<std::string> seed_nodes;
        std::vector<int> shard_ids;
        int replication_factor;
    };
    
    // Index configuration
    struct IndexConfig {
        int dimension;
        std::string type;
        std::unordered_map<std::string, std::string> parameters;
        std::string storage_path;
        bool auto_save;
        int save_interval_seconds;
    };
    
    // Performance configuration
    struct PerformanceConfig {
        int batch_size;
        int max_concurrent_loads;
        int max_concurrent_queries;
        bool cache_enabled;
        size_t cache_size;
        int query_timeout_ms;
        int load_timeout_ms;
    };
    
    // Network configuration
    struct NetworkConfig {
        int max_connections;
        int connection_timeout_ms;
        int read_timeout_ms;
        int write_timeout_ms;
        bool compression_enabled;
        int max_retries;
        std::string load_balance_strategy;
    };
    
    // Storage configuration
    struct StorageConfig {
        std::string type; // "redis", "local", "hybrid"
        std::string redis_host;
        int redis_port;
        int redis_db;
        std::string local_storage_path;
        size_t local_cache_size;
        bool compression_enabled;
        bool encryption_enabled;
        std::string encryption_key;
    };
    
    // Logging configuration
    struct LoggingConfig {
        std::string level;
        std::string output_file;
        bool console_output;
        size_t max_file_size_mb;
        int max_files;
        std::string pattern;
    };
    
    // Getters
    NodeConfig get_node_config() const;
    IndexConfig get_index_config() const;
    PerformanceConfig get_performance_config() const;
    NetworkConfig get_network_config() const;
    StorageConfig get_storage_config() const;
    LoggingConfig get_logging_config() const;
    
    // Setters
    void set_node_config(const NodeConfig& config);
    void set_index_config(const IndexConfig& config);
    void set_performance_config(const PerformanceConfig& config);
    void set_network_config(const NetworkConfig& config);
    void set_storage_config(const StorageConfig& config);
    void set_logging_config(const LoggingConfig& config);
    
    // Individual value access
    std::string get_string(const std::string& key, const std::string& default_value = "") const;
    int get_int(const std::string& key, int default_value = 0) const;
    bool get_bool(const std::string& key, bool default_value = false) const;
    double get_double(const std::string& key, double default_value = 0.0) const;
    std::vector<std::string> get_string_list(const std::string& key, const std::vector<std::string>& default_value = {}) const;
    
    void set_string(const std::string& key, const std::string& value);
    void set_int(const std::string& key, int value);
    void set_bool(const std::string& key, bool value);
    void set_double(const std::string& key, double value);
    void set_string_list(const std::string& key, const std::vector<std::string>& value);
    
    // Configuration validation
    bool validate() const;
    std::vector<std::string> get_validation_errors() const;
    
    // Configuration merging
    bool merge_with_file(const std::string& filename);
    bool merge_with_string(const std::string& json_string);
    
    // Environment variable substitution
    void substitute_env_vars();
    
    // Configuration sections
    std::vector<std::string> get_sections() const;
    bool has_section(const std::string& section) const;
    std::unordered_map<std::string, std::string> get_section(const std::string& section) const;
    
private:
    Config();
    ~Config();
    
    // Prevent copying
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    
    mutable std::mutex config_mutex_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> config_data_;
    
    // Internal methods
    bool parse_json(const std::string& json_string);
    std::string to_json() const;
    
    // Validation helpers
    bool validate_node_config(const NodeConfig& config) const;
    bool validate_index_config(const IndexConfig& config) const;
    bool validate_performance_config(const PerformanceConfig& config) const;
    bool validate_network_config(const NetworkConfig& config) const;
    bool validate_storage_config(const StorageConfig& config) const;
    bool validate_logging_config(const LoggingConfig& config) const;
    
    // Default configurations
    NodeConfig get_default_node_config() const;
    IndexConfig get_default_index_config() const;
    PerformanceConfig get_default_performance_config() const;
    NetworkConfig get_default_network_config() const;
    StorageConfig get_default_storage_config() const;
    LoggingConfig get_default_logging_config() const;
    
    // Configuration parsing helpers
    std::string get_nested_value(const std::vector<std::string>& keys, const std::string& default_value = "") const;
    void set_nested_value(const std::vector<std::string>& keys, const std::string& value);
    
    // Environment variable helpers
    std::string expand_env_var(const std::string& value) const;
    bool is_env_var(const std::string& str) const;
    std::string extract_env_var_name(const std::string& str) const;
};

// Convenience macros for accessing configuration
#define CONFIG_GET_STRING(key) Config::instance().get_string(key)
#define CONFIG_GET_INT(key) Config::instance().get_int(key)
#define CONFIG_GET_BOOL(key) Config::instance().get_bool(key)
#define CONFIG_GET_DOUBLE(key) Config::instance().get_double(key)

#define CONFIG_SET_STRING(key, value) Config::instance().set_string(key, value)
#define CONFIG_SET_INT(key, value) Config::instance().set_int(key, value)
#define CONFIG_SET_BOOL(key, value) Config::instance().set_bool(key, value)
#define CONFIG_SET_DOUBLE(key, value) Config::instance().set_double(key, value)

} // namespace dann
