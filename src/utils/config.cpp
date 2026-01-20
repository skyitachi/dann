#include "dann/config.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <regex>

namespace dann {

Config& Config::instance() {
    static Config instance;
    return instance;
}

Config::Config() {
    // Initialize with default configurations
    set_default_configurations();
}

Config::~Config() = default;

bool Config::load_from_file(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    file.close();
    
    return load_from_string(content);
}

bool Config::load_from_string(const std::string& json_string) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return parse_json(json_string);
}

bool Config::load_from_env() {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    // Load common environment variables
    const char* node_id = std::getenv("DANN_NODE_ID");
    if (node_id) {
        config_data_["node"]["id"] = node_id;
    }
    
    const char* node_address = std::getenv("DANN_NODE_ADDRESS");
    if (node_address) {
        config_data_["node"]["address"] = node_address;
    }
    
    const char* node_port = std::getenv("DANN_NODE_PORT");
    if (node_port) {
        config_data_["node"]["port"] = node_port;
    }
    
    const char* index_dimension = std::getenv("DANN_INDEX_DIMENSION");
    if (index_dimension) {
        config_data_["index"]["dimension"] = index_dimension;
    }
    
    const char* index_type = std::getenv("DANN_INDEX_TYPE");
    if (index_type) {
        config_data_["index"]["type"] = index_type;
    }
    
    return true;
}

bool Config::save_to_file(const std::string& filename) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    std::string json_content = to_json();
    
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    file << json_content;
    return file.good();
}

std::string Config::to_string() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return to_json();
}

Config::NodeConfig Config::get_node_config() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    NodeConfig config;
    config.id = get_nested_value({"node", "id"}, get_default_node_config().id);
    config.address = get_nested_value({"node", "address"}, get_default_node_config().address);
    config.port = get_int(get_nested_value({"node", "port"}, std::to_string(get_default_node_config().port)));
    
    // Parse seed nodes
    std::string seeds_str = get_nested_value({"node", "seed_nodes"}, "");
    if (!seeds_str.empty()) {
        std::stringstream ss(seeds_str);
        std::string seed;
        while (std::getline(ss, seed, ',')) {
            config.seed_nodes.push_back(seed);
        }
    }
    
    // Parse shard IDs
    std::string shards_str = get_nested_value({"node", "shard_ids"}, "");
    if (!shards_str.empty()) {
        std::stringstream ss(shards_str);
        std::string shard;
        while (std::getline(ss, shard, ',')) {
            config.shard_ids.push_back(std::stoi(shard));
        }
    }
    
    config.replication_factor = get_int(get_nested_value({"node", "replication_factor"}, 
                                                       std::to_string(get_default_node_config().replication_factor)));
    
    return config;
}

Config::IndexConfig Config::get_index_config() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    IndexConfig config;
    config.dimension = get_int(get_nested_value({"index", "dimension"}, 
                                             std::to_string(get_default_index_config().dimension)));
    config.type = get_nested_value({"index", "type"}, get_default_index_config().type);
    config.storage_path = get_nested_value({"index", "storage_path"}, get_default_index_config().storage_path);
    config.auto_save = get_bool(get_nested_value({"index", "auto_save"}, 
                                               get_default_index_config().auto_save ? "true" : "false"));
    config.save_interval_seconds = get_int(get_nested_value({"index", "save_interval_seconds"}, 
                                                          std::to_string(get_default_index_config().save_interval_seconds)));
    
    return config;
}

Config::PerformanceConfig Config::get_performance_config() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    PerformanceConfig config;
    config.batch_size = get_int(get_nested_value({"performance", "batch_size"}, 
                                               std::to_string(get_default_performance_config().batch_size)));
    config.max_concurrent_loads = get_int(get_nested_value({"performance", "max_concurrent_loads"}, 
                                                          std::to_string(get_default_performance_config().max_concurrent_loads)));
    config.max_concurrent_queries = get_int(get_nested_value({"performance", "max_concurrent_queries"}, 
                                                           std::to_string(get_default_performance_config().max_concurrent_queries)));
    config.cache_enabled = get_bool(get_nested_value({"performance", "cache_enabled"}, 
                                                    get_default_performance_config().cache_enabled ? "true" : "false"));
    config.cache_size = get_int(get_nested_value({"performance", "cache_size"}, 
                                                std::to_string(get_default_performance_config().cache_size)));
    config.query_timeout_ms = get_int(get_nested_value({"performance", "query_timeout_ms"}, 
                                                      std::to_string(get_default_performance_config().query_timeout_ms)));
    config.load_timeout_ms = get_int(get_nested_value({"performance", "load_timeout_ms"}, 
                                                     std::to_string(get_default_performance_config().load_timeout_ms)));
    
    return config;
}

Config::NetworkConfig Config::get_network_config() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    NetworkConfig config;
    config.max_connections = get_int(get_nested_value({"network", "max_connections"}, 
                                                    std::to_string(get_default_network_config().max_connections)));
    config.connection_timeout_ms = get_int(get_nested_value({"network", "connection_timeout_ms"}, 
                                                           std::to_string(get_default_network_config().connection_timeout_ms)));
    config.read_timeout_ms = get_int(get_nested_value({"network", "read_timeout_ms"}, 
                                                     std::to_string(get_default_network_config().read_timeout_ms)));
    config.write_timeout_ms = get_int(get_nested_value({"network", "write_timeout_ms"}, 
                                                      std::to_string(get_default_network_config().write_timeout_ms)));
    config.compression_enabled = get_bool(get_nested_value({"network", "compression_enabled"}, 
                                                         get_default_network_config().compression_enabled ? "true" : "false"));
    config.max_retries = get_int(get_nested_value({"network", "max_retries"}, 
                                                 std::to_string(get_default_network_config().max_retries)));
    config.load_balance_strategy = get_nested_value({"network", "load_balance_strategy"}, 
                                                  get_default_network_config().load_balance_strategy);
    
    return config;
}

Config::StorageConfig Config::get_storage_config() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    StorageConfig config;
    config.type = get_nested_value({"storage", "type"}, get_default_storage_config().type);
    config.redis_host = get_nested_value({"storage", "redis_host"}, get_default_storage_config().redis_host);
    config.redis_port = get_int(get_nested_value({"storage", "redis_port"}, 
                                                std::to_string(get_default_storage_config().redis_port)));
    config.redis_db = get_int(get_nested_value({"storage", "redis_db"}, 
                                             std::to_string(get_default_storage_config().redis_db)));
    config.local_storage_path = get_nested_value({"storage", "local_storage_path"}, 
                                               get_default_storage_config().local_storage_path);
    config.local_cache_size = get_int(get_nested_value({"storage", "local_cache_size"}, 
                                                     std::to_string(get_default_storage_config().local_cache_size)));
    config.compression_enabled = get_bool(get_nested_value({"storage", "compression_enabled"}, 
                                                         get_default_storage_config().compression_enabled ? "true" : "false"));
    config.encryption_enabled = get_bool(get_nested_value({"storage", "encryption_enabled"}, 
                                                        get_default_storage_config().encryption_enabled ? "true" : "false"));
    config.encryption_key = get_nested_value({"storage", "encryption_key"}, get_default_storage_config().encryption_key);
    
    return config;
}

Config::LoggingConfig Config::get_logging_config() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    LoggingConfig config;
    config.level = get_nested_value({"logging", "level"}, get_default_logging_config().level);
    config.output_file = get_nested_value({"logging", "output_file"}, get_default_logging_config().output_file);
    config.console_output = get_bool(get_nested_value({"logging", "console_output"}, 
                                                     get_default_logging_config().console_output ? "true" : "false"));
    config.max_file_size_mb = get_int(get_nested_value({"logging", "max_file_size_mb"}, 
                                                      std::to_string(get_default_logging_config().max_file_size_mb)));
    config.max_files = get_int(get_nested_value({"logging", "max_files"}, 
                                               std::to_string(get_default_logging_config().max_files)));
    config.pattern = get_nested_value({"logging", "pattern"}, get_default_logging_config().pattern);
    
    return config;
}

void Config::set_node_config(const NodeConfig& config) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    config_data_["node"]["id"] = config.id;
    config_data_["node"]["address"] = config.address;
    config_data_["node"]["port"] = std::to_string(config.port);
    
    // Convert seed nodes to comma-separated string
    std::stringstream ss;
    for (size_t i = 0; i < config.seed_nodes.size(); ++i) {
        if (i > 0) ss << ",";
        ss << config.seed_nodes[i];
    }
    config_data_["node"]["seed_nodes"] = ss.str();
    
    // Convert shard IDs to comma-separated string
    ss.str("");
    for (size_t i = 0; i < config.shard_ids.size(); ++i) {
        if (i > 0) ss << ",";
        ss << config.shard_ids[i];
    }
    config_data_["node"]["shard_ids"] = ss.str();
    
    config_data_["node"]["replication_factor"] = std::to_string(config.replication_factor);
}

void Config::set_index_config(const IndexConfig& config) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    config_data_["index"]["dimension"] = std::to_string(config.dimension);
    config_data_["index"]["type"] = config.type;
    config_data_["index"]["storage_path"] = config.storage_path;
    config_data_["index"]["auto_save"] = config.auto_save ? "true" : "false";
    config_data_["index"]["save_interval_seconds"] = std::to_string(config.save_interval_seconds);
}

void Config::set_performance_config(const PerformanceConfig& config) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    config_data_["performance"]["batch_size"] = std::to_string(config.batch_size);
    config_data_["performance"]["max_concurrent_loads"] = std::to_string(config.max_concurrent_loads);
    config_data_["performance"]["max_concurrent_queries"] = std::to_string(config.max_concurrent_queries);
    config_data_["performance"]["cache_enabled"] = config.cache_enabled ? "true" : "false";
    config_data_["performance"]["cache_size"] = std::to_string(config.cache_size);
    config_data_["performance"]["query_timeout_ms"] = std::to_string(config.query_timeout_ms);
    config_data_["performance"]["load_timeout_ms"] = std::to_string(config.load_timeout_ms);
}

void Config::set_network_config(const NetworkConfig& config) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    config_data_["network"]["max_connections"] = std::to_string(config.max_connections);
    config_data_["network"]["connection_timeout_ms"] = std::to_string(config.connection_timeout_ms);
    config_data_["network"]["read_timeout_ms"] = std::to_string(config.read_timeout_ms);
    config_data_["network"]["write_timeout_ms"] = std::to_string(config.write_timeout_ms);
    config_data_["network"]["compression_enabled"] = config.compression_enabled ? "true" : "false";
    config_data_["network"]["max_retries"] = std::to_string(config.max_retries);
    config_data_["network"]["load_balance_strategy"] = config.load_balance_strategy;
}

void Config::set_storage_config(const StorageConfig& config) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    config_data_["storage"]["type"] = config.type;
    config_data_["storage"]["redis_host"] = config.redis_host;
    config_data_["storage"]["redis_port"] = std::to_string(config.redis_port);
    config_data_["storage"]["redis_db"] = std::to_string(config.redis_db);
    config_data_["storage"]["local_storage_path"] = config.local_storage_path;
    config_data_["storage"]["local_cache_size"] = std::to_string(config.local_cache_size);
    config_data_["storage"]["compression_enabled"] = config.compression_enabled ? "true" : "false";
    config_data_["storage"]["encryption_enabled"] = config.encryption_enabled ? "true" : "false";
    config_data_["storage"]["encryption_key"] = config.encryption_key;
}

void Config::set_logging_config(const LoggingConfig& config) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    config_data_["logging"]["level"] = config.level;
    config_data_["logging"]["output_file"] = config.output_file;
    config_data_["logging"]["console_output"] = config.console_output ? "true" : "false";
    config_data_["logging"]["max_file_size_mb"] = std::to_string(config.max_file_size_mb);
    config_data_["logging"]["max_files"] = std::to_string(config.max_files);
    config_data_["logging"]["pattern"] = config.pattern;
}

std::string Config::get_string(const std::string& key, const std::string& default_value) const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    size_t dot_pos = key.find('.');
    if (dot_pos != std::string::npos) {
        std::string section = key.substr(0, dot_pos);
        std::string subsection = key.substr(dot_pos + 1);
        
        auto section_it = config_data_.find(section);
        if (section_it != config_data_.end()) {
            auto subsection_it = section_it->second.find(subsection);
            if (subsection_it != section_it->second.end()) {
                return subsection_it->second;
            }
        }
    }
    
    return default_value;
}

int Config::get_int(const std::string& key, int default_value) const {
    std::string value = get_string(key, std::to_string(default_value));
    try {
        return std::stoi(value);
    } catch (const std::exception&) {
        return default_value;
    }
}

bool Config::get_bool(const std::string& key, bool default_value) const {
    std::string value = get_string(key, default_value ? "true" : "false");
    std::transform(value.begin(), value.end(), value.begin(), ::tolower);
    
    return value == "true" || value == "1" || value == "yes" || value == "on";
}

double Config::get_double(const std::string& key, double default_value) const {
    std::string value = get_string(key, std::to_string(default_value));
    try {
        return std::stod(value);
    } catch (const std::exception&) {
        return default_value;
    }
}

std::vector<std::string> Config::get_string_list(const std::string& key, const std::vector<std::string>& default_value) const {
    std::string value = get_string(key, "");
    if (value.empty()) {
        return default_value;
    }
    
    std::vector<std::string> result;
    std::stringstream ss(value);
    std::string item;
    
    while (std::getline(ss, item, ',')) {
        // Trim whitespace
        item.erase(0, item.find_first_not_of(" \t"));
        item.erase(item.find_last_not_of(" \t") + 1);
        
        if (!item.empty()) {
            result.push_back(item);
        }
    }
    
    return result;
}

void Config::set_string(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    set_nested_value({key}, value);
}

void Config::set_int(const std::string& key, int value) {
    set_string(key, std::to_string(value));
}

void Config::set_bool(const std::string& key, bool value) {
    set_string(key, value ? "true" : "false");
}

void Config::set_double(const std::string& key, double value) {
    set_string(key, std::to_string(value));
}

void Config::set_string_list(const std::string& key, const std::vector<std::string>& value) {
    std::stringstream ss;
    for (size_t i = 0; i < value.size(); ++i) {
        if (i > 0) ss << ",";
        ss << value[i];
    }
    set_string(key, ss.str());
}

bool Config::validate() const {
    auto errors = get_validation_errors();
    return errors.empty();
}

std::vector<std::string> Config::get_validation_errors() const {
    std::vector<std::string> errors;
    
    // Validate node config
    auto node_config = get_node_config();
    if (!validate_node_config(node_config)) {
        errors.push_back("Invalid node configuration");
    }
    
    // Validate index config
    auto index_config = get_index_config();
    if (!validate_index_config(index_config)) {
        errors.push_back("Invalid index configuration");
    }
    
    // Validate performance config
    auto perf_config = get_performance_config();
    if (!validate_performance_config(perf_config)) {
        errors.push_back("Invalid performance configuration");
    }
    
    // Validate network config
    auto network_config = get_network_config();
    if (!validate_network_config(network_config)) {
        errors.push_back("Invalid network configuration");
    }
    
    // Validate storage config
    auto storage_config = get_storage_config();
    if (!validate_storage_config(storage_config)) {
        errors.push_back("Invalid storage configuration");
    }
    
    // Validate logging config
    auto logging_config = get_logging_config();
    if (!validate_logging_config(logging_config)) {
        errors.push_back("Invalid logging configuration");
    }
    
    return errors;
}

bool Config::merge_with_file(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    file.close();
    
    return merge_with_string(content);
}

bool Config::merge_with_string(const std::string& json_string) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return parse_json(json_string);
}

void Config::substitute_env_vars() {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    for (auto& section_pair : config_data_) {
        for (auto& key_value_pair : section_pair.second) {
            key_value_pair.second = expand_env_var(key_value_pair.second);
        }
    }
}

std::vector<std::string> Config::get_sections() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    std::vector<std::string> sections;
    for (const auto& pair : config_data_) {
        sections.push_back(pair.first);
    }
    
    return sections;
}

bool Config::has_section(const std::string& section) const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return config_data_.find(section) != config_data_.end();
}

std::unordered_map<std::string, std::string> Config::get_section(const std::string& section) const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    auto it = config_data_.find(section);
    if (it != config_data_.end()) {
        return it->second;
    }
    
    return {};
}

bool Config::parse_json(const std::string& json_string) {
    // Simplified JSON parsing - in real implementation would use a proper JSON library
    // For now, just return true
    return true;
}

std::string Config::to_json() const {
    // Simplified JSON generation - in real implementation would use a proper JSON library
    std::stringstream ss;
    ss << "{\n";
    
    for (const auto& section_pair : config_data_) {
        ss << "  \"" << section_pair.first << "\": {\n";
        
        for (const auto& key_value_pair : section_pair.second) {
            ss << "    \"" << key_value_pair.first << "\": \"" << key_value_pair.second << "\"";
            
            // Add comma if not last item
            auto it = section_pair.second.find(key_value_pair.first);
            ++it;
            if (it != section_pair.second.end()) {
                ss << ",";
            }
            ss << "\n";
        }
        
        ss << "  }";
        
        // Add comma if not last section
        auto it = config_data_.find(section_pair.first);
        ++it;
        if (it != config_data_.end()) {
            ss << ",";
        }
        ss << "\n";
    }
    
    ss << "}\n";
    return ss.str();
}

std::string Config::get_nested_value(const std::vector<std::string>& keys, const std::string& default_value) const {
    if (keys.empty()) {
        return default_value;
    }
    
    auto section_it = config_data_.find(keys[0]);
    if (section_it == config_data_.end()) {
        return default_value;
    }
    
    if (keys.size() == 1) {
        // Return first key in section as a string (simplified)
        if (!section_it->second.empty()) {
            return section_it->second.begin()->second;
        }
        return default_value;
    }
    
    auto subsection_it = section_it->second.find(keys[1]);
    if (subsection_it == section_it->second.end()) {
        return default_value;
    }
    
    return subsection_it->second;
}

void Config::set_nested_value(const std::vector<std::string>& keys, const std::string& value) {
    if (keys.size() >= 1) {
        config_data_[keys[0]][keys.size() > 1 ? keys[1] : "value"] = value;
    }
}

std::string Config::expand_env_var(const std::string& value) const {
    if (!is_env_var(value)) {
        return value;
    }
    
    std::string var_name = extract_env_var_name(value);
    const char* env_value = std::getenv(var_name.c_str());
    
    return env_value ? env_value : value;
}

bool Config::is_env_var(const std::string& str) const {
    return str.find("${") == 0 && str.find("}") == str.length() - 1;
}

std::string Config::extract_env_var_name(const std::string& str) const {
    if (str.length() > 3 && str[0] == '$' && str[1] == '{' && str.back() == '}') {
        return str.substr(2, str.length() - 3);
    }
    return str;
}

// Validation methods
bool Config::validate_node_config(const NodeConfig& config) const {
    return !config.id.empty() && config.port > 0 && config.port <= 65535;
}

bool Config::validate_index_config(const IndexConfig& config) const {
    return config.dimension > 0 && !config.type.empty();
}

bool Config::validate_performance_config(const PerformanceConfig& config) const {
    return config.batch_size > 0 && config.max_concurrent_loads > 0 && config.max_concurrent_queries > 0;
}

bool Config::validate_network_config(const NetworkConfig& config) const {
    return config.max_connections > 0 && config.connection_timeout_ms > 0;
}

bool Config::validate_storage_config(const StorageConfig& config) const {
    return !config.type.empty();
}

bool Config::validate_logging_config(const LoggingConfig& config) const {
    return !config.level.empty();
}

// Default configurations
void Config::set_default_configurations() {
    auto node_config = get_default_node_config();
    set_node_config(node_config);
    
    auto index_config = get_default_index_config();
    set_index_config(index_config);
    
    auto perf_config = get_default_performance_config();
    set_performance_config(perf_config);
    
    auto network_config = get_default_network_config();
    set_network_config(network_config);
    
    auto storage_config = get_default_storage_config();
    set_storage_config(storage_config);
    
    auto logging_config = get_default_logging_config();
    set_logging_config(logging_config);
}

Config::NodeConfig Config::get_default_node_config() const {
    NodeConfig config;
    config.id = "node1";
    config.address = "0.0.0.0";
    config.port = 8080;
    config.replication_factor = 3;
    return config;
}

Config::IndexConfig Config::get_default_index_config() const {
    IndexConfig config;
    config.dimension = 128;
    config.type = "IVF";
    config.storage_path = "./data";
    config.auto_save = true;
    config.save_interval_seconds = 300;
    return config;
}

Config::PerformanceConfig Config::get_default_performance_config() const {
    PerformanceConfig config;
    config.batch_size = 1000;
    config.max_concurrent_loads = 4;
    config.max_concurrent_queries = 100;
    config.cache_enabled = true;
    config.cache_size = 10000;
    config.query_timeout_ms = 5000;
    config.load_timeout_ms = 30000;
    return config;
}

Config::NetworkConfig Config::get_default_network_config() const {
    NetworkConfig config;
    config.max_connections = 1000;
    config.connection_timeout_ms = 5000;
    config.read_timeout_ms = 10000;
    config.write_timeout_ms = 10000;
    config.compression_enabled = false;
    config.max_retries = 3;
    config.load_balance_strategy = "round_robin";
    return config;
}

Config::StorageConfig Config::get_default_storage_config() const {
    StorageConfig config;
    config.type = "local";
    config.redis_host = "localhost";
    config.redis_port = 6379;
    config.redis_db = 0;
    config.local_storage_path = "./data";
    config.local_cache_size = 1000;
    config.compression_enabled = false;
    config.encryption_enabled = false;
    return config;
}

Config::LoggingConfig Config::get_default_logging_config() const {
    LoggingConfig config;
    config.level = "INFO";
    config.output_file = "./logs/dann.log";
    config.console_output = true;
    config.max_file_size_mb = 100;
    config.max_files = 5;
    config.pattern = "[%Y-%m-%d %H:%M:%S] [%l] %v";
    return config;
}

} // namespace dann
