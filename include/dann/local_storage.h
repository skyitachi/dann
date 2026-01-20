#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <fstream>
#include "dann/types.h"

namespace dann {

class LocalStorage {
public:
    LocalStorage(const std::string& data_dir = "./data");
    ~LocalStorage();
    
    // Storage management
    bool initialize();
    bool cleanup();
    
    // Basic key-value operations
    bool set(const std::string& key, const std::string& value);
    std::string get(const std::string& key);
    bool del(const std::string& key);
    bool exists(const std::string& key);
    
    // Vector storage
    bool set_vector(const std::string& key, const std::vector<float>& vector);
    std::vector<float> get_vector(const std::string& key);
    bool del_vector(const std::string& key);
    
    // Batch operations
    bool set_batch(const std::vector<std::pair<std::string, std::string>>& key_values);
    std::vector<std::string> get_batch(const std::vector<std::string>& keys);
    
    // Index operations
    bool save_index(const std::string& index_name, const std::vector<uint8_t>& index_data);
    std::vector<uint8_t> load_index(const std::string& index_name);
    bool delete_index(const std::string& index_name);
    std::vector<std::string> list_indices();
    
    // Metadata operations
    bool set_metadata(const std::string& key, const std::string& metadata);
    std::string get_metadata(const std::string& key);
    
    // Persistence
    bool flush_to_disk();
    bool load_from_disk();
    
    // Backup and restore
    bool backup(const std::string& backup_path);
    bool restore(const std::string& backup_path);
    
    // Configuration
    void set_cache_size(size_t cache_size);
    void set_compression_enabled(bool enabled);
    void set_encryption_enabled(bool enabled, const std::string& key = "");
    
    // Statistics
    struct StorageStats {
        uint64_t total_keys;
        uint64_t total_vectors;
        uint64_t total_size_bytes;
        uint64_t cache_hits;
        uint64_t cache_misses;
        double cache_hit_ratio;
        uint64_t disk_reads;
        uint64_t disk_writes;
    };
    
    StorageStats get_stats() const;
    void reset_stats();
    
    // Maintenance
    bool compact();
    bool verify_integrity();
    bool cleanup_expired();
    
private:
    std::string data_dir_;
    size_t cache_size_;
    bool compression_enabled_;
    bool encryption_enabled_;
    std::string encryption_key_;
    
    mutable std::mutex storage_mutex_;
    std::unordered_map<std::string, std::string> memory_cache_;
    std::unordered_map<std::string, std::vector<float>> vector_cache_;
    
    mutable std::mutex stats_mutex_;
    StorageStats stats_;
    
    // File operations
    std::string get_file_path(const std::string& key);
    bool write_file(const std::string& path, const std::string& data);
    std::string read_file(const std::string& path);
    bool delete_file(const std::string& path);
    
    // Directory operations
    bool create_directory(const std::string& path);
    std::vector<std::string> list_files(const std::string& path);
    
    // Cache management
    void evict_from_cache();
    bool is_in_cache(const std::string& key);
    void add_to_cache(const std::string& key, const std::string& value);
    
    // Compression
    std::string compress_data(const std::string& data);
    std::string decompress_data(const std::string& compressed_data);
    
    // Encryption
    std::string encrypt_data(const std::string& data);
    std::string decrypt_data(const std::string& encrypted_data);
    
    // Serialization
    std::string serialize_vector(const std::vector<float>& vector);
    std::vector<float> deserialize_vector(const std::string& data);
    
    // Statistics
    void update_stats(bool cache_hit, bool disk_operation);
    
    // Utilities
    std::string generate_hash(const std::string& key);
    bool validate_key(const std::string& key);
};

} // namespace dann
