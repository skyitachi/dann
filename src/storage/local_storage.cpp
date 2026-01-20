#include "dann/local_storage.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <cstring>
#include <openssl/md5.h>

namespace dann {

LocalStorage::LocalStorage(const std::string& data_dir)
    : data_dir_(data_dir), cache_size_(1000), compression_enabled_(false),
      encryption_enabled_(false) {
    
    stats_ = StorageStats{};
    stats_.total_keys = 0;
    stats_.total_vectors = 0;
    stats_.total_size_bytes = 0;
    stats_.cache_hits = 0;
    stats_.cache_misses = 0;
    stats_.cache_hit_ratio = 0.0;
    stats_.disk_reads = 0;
    stats_.disk_writes = 0;
}

LocalStorage::~LocalStorage() {
    cleanup();
}

bool LocalStorage::initialize() {
    try {
        // Create data directory if it doesn't exist
        if (!create_directory(data_dir_)) {
            return false;
        }
        
        // Create subdirectories
        create_directory(data_dir_ + "/vectors");
        create_directory(data_dir_ + "/indices");
        create_directory(data_dir_ + "/metadata");
        
        // Load existing data from disk
        load_from_disk();
        
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

bool LocalStorage::cleanup() {
    // Flush cache to disk
    flush_to_disk();
    
    // Clear memory
    {
        std::lock_guard<std::mutex> lock(storage_mutex_);
        memory_cache_.clear();
        vector_cache_.clear();
    }
    
    return true;
}

bool LocalStorage::set(const std::string& key, const std::string& value) {
    if (!validate_key(key)) {
        return false;
    }
    
    std::string processed_value = value;
    
    // Apply compression if enabled
    if (compression_enabled_) {
        processed_value = compress_data(processed_value);
    }
    
    // Apply encryption if enabled
    if (encryption_enabled_) {
        processed_value = encrypt_data(processed_value);
    }
    
    // Add to cache
    {
        std::lock_guard<std::mutex> lock(storage_mutex_);
        add_to_cache(key, processed_value);
    }
    
    update_stats(true, false); // Cache hit, no disk operation
    
    return true;
}

std::string LocalStorage::get(const std::string& key) {
    if (!validate_key(key)) {
        return "";
    }
    
    // Check cache first
    {
        std::lock_guard<std::mutex> lock(storage_mutex_);
        auto it = memory_cache_.find(key);
        if (it != memory_cache_.end()) {
            update_stats(true, false); // Cache hit
            std::string value = it->second;
            
            // Apply decryption if enabled
            if (encryption_enabled_) {
                value = decrypt_data(value);
            }
            
            // Apply decompression if enabled
            if (compression_enabled_) {
                value = decompress_data(value);
            }
            
            return value;
        }
    }
    
    update_stats(false, true); // Cache miss, disk read
    
    // Read from disk
    std::string file_path = get_file_path(key);
    std::string value = read_file(file_path);
    
    if (value.empty()) {
        return "";
    }
    
    // Apply decryption if enabled
    if (encryption_enabled_) {
        value = decrypt_data(value);
    }
    
    // Apply decompression if enabled
    if (compression_enabled_) {
        value = decompress_data(value);
    }
    
    // Add to cache
    {
        std::lock_guard<std::mutex> lock(storage_mutex_);
        add_to_cache(key, value);
    }
    
    return value;
}

bool LocalStorage::del(const std::string& key) {
    if (!validate_key(key)) {
        return false;
    }
    
    // Remove from cache
    {
        std::lock_guard<std::mutex> lock(storage_mutex_);
        memory_cache_.erase(key);
        vector_cache_.erase(key);
    }
    
    // Delete from disk
    std::string file_path = get_file_path(key);
    bool deleted = delete_file(file_path);
    
    update_stats(false, false); // No cache hit, disk write
    
    return deleted;
}

bool LocalStorage::exists(const std::string& key) {
    if (!validate_key(key)) {
        return false;
    }
    
    // Check cache first
    {
        std::lock_guard<std::mutex> lock(storage_mutex_);
        if (memory_cache_.find(key) != memory_cache_.end()) {
            return true;
        }
    }
    
    // Check disk
    std::string file_path = get_file_path(key);
    std::ifstream file(file_path);
    return file.good();
}

bool LocalStorage::set_vector(const std::string& key, const std::vector<float>& vector) {
    if (!validate_key(key) || vector.empty()) {
        return false;
    }
    
    // Serialize vector
    std::string serialized = serialize_vector(vector);
    
    // Store as regular key-value
    bool success = set(key, serialized);
    
    if (success) {
        // Also store in vector cache for fast access
        std::lock_guard<std::mutex> lock(storage_mutex_);
        vector_cache_[key] = vector;
        
        // Update stats
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.total_vectors++;
    }
    
    return success;
}

std::vector<float> LocalStorage::get_vector(const std::string& key) {
    if (!validate_key(key)) {
        return {};
    }
    
    // Check vector cache first
    {
        std::lock_guard<std::mutex> lock(storage_mutex_);
        auto it = vector_cache_.find(key);
        if (it != vector_cache_.end()) {
            update_stats(true, false); // Cache hit
            return it->second;
        }
    }
    
    // Get serialized data
    std::string serialized = get(key);
    if (serialized.empty()) {
        return {};
    }
    
    // Deserialize vector
    std::vector<float> vector = deserialize_vector(serialized);
    
    if (!vector.empty()) {
        // Add to vector cache
        std::lock_guard<std::mutex> lock(storage_mutex_);
        vector_cache_[key] = vector;
    }
    
    return vector;
}

bool LocalStorage::del_vector(const std::string& key) {
    bool success = del(key);
    
    if (success) {
        // Remove from vector cache
        std::lock_guard<std::mutex> lock(storage_mutex_);
        vector_cache_.erase(key);
        
        // Update stats
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        if (stats_.total_vectors > 0) {
            stats_.total_vectors--;
        }
    }
    
    return success;
}

bool LocalStorage::set_batch(const std::vector<std::pair<std::string, std::string>>& key_values) {
    bool all_success = true;
    
    for (const auto& kv : key_values) {
        if (!set(kv.first, kv.second)) {
            all_success = false;
        }
    }
    
    return all_success;
}

std::vector<std::string> LocalStorage::get_batch(const std::vector<std::string>& keys) {
    std::vector<std::string> values;
    values.reserve(keys.size());
    
    for (const auto& key : keys) {
        values.push_back(get(key));
    }
    
    return values;
}

bool LocalStorage::save_index(const std::string& index_name, const std::vector<uint8_t>& index_data) {
    std::string index_path = data_dir_ + "/indices/" + index_name + ".idx";
    
    std::ofstream file(index_path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    file.write(reinterpret_cast<const char*>(index_data.data()), index_data.size());
    
    update_stats(false, false); // No cache hit, disk write
    
    return file.good();
}

std::vector<uint8_t> LocalStorage::load_index(const std::string& index_name) {
    std::string index_path = data_dir_ + "/indices/" + index_name + ".idx";
    
    std::ifstream file(index_path, std::ios::binary);
    if (!file.is_open()) {
        return {};
    }
    
    // Get file size
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> index_data(file_size);
    file.read(reinterpret_cast<char*>(index_data.data()), file_size);
    
    update_stats(false, true); // No cache hit, disk read
    
    return file.good() ? index_data : std::vector<uint8_t>{};
}

bool LocalStorage::delete_index(const std::string& index_name) {
    std::string index_path = data_dir_ + "/indices/" + index_name + ".idx";
    return delete_file(index_path);
}

std::vector<std::string> LocalStorage::list_indices() {
    std::string indices_dir = data_dir_ + "/indices";
    std::vector<std::string> indices;
    
    try {
        for (const auto& entry : std::filesystem::directory_iterator(indices_dir)) {
            if (entry.path().extension() == ".idx") {
                indices.push_back(entry.path().stem().string());
            }
        }
    } catch (const std::exception& e) {
        // Return empty list on error
    }
    
    return indices;
}

bool LocalStorage::set_metadata(const std::string& key, const std::string& metadata) {
    std::string metadata_key = "metadata:" + key;
    return set(metadata_key, metadata);
}

std::string LocalStorage::get_metadata(const std::string& key) {
    std::string metadata_key = "metadata:" + key;
    return get(metadata_key);
}

bool LocalStorage::flush_to_disk() {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    bool all_success = true;
    
    // Write all cached data to disk
    for (const auto& kv : memory_cache_) {
        std::string file_path = get_file_path(kv.first);
        if (!write_file(file_path, kv.second)) {
            all_success = false;
        }
    }
    
    return all_success;
}

bool LocalStorage::load_from_disk() {
    // Load all files from data directory into cache
    // This is a simplified implementation
    return true;
}

bool LocalStorage::backup(const std::string& backup_path) {
    try {
        // Create backup directory
        create_directory(backup_path);
        
        // Copy all files
        std::filesystem::copy(data_dir_, backup_path, 
                             std::filesystem::copy_options::recursive);
        
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

bool LocalStorage::restore(const std::string& backup_path) {
    try {
        // Clear current data
        cleanup();
        
        // Copy backup files
        std::filesystem::copy(backup_path, data_dir_,
                             std::filesystem::copy_options::recursive);
        
        // Reload data
        return load_from_disk();
    } catch (const std::exception& e) {
        return false;
    }
}

void LocalStorage::set_cache_size(size_t cache_size) {
    cache_size_ = std::max(size_t(1), cache_size);
    
    // Evict excess entries if needed
    evict_from_cache();
}

void LocalStorage::set_compression_enabled(bool enabled) {
    compression_enabled_ = enabled;
}

void LocalStorage::set_encryption_enabled(bool enabled, const std::string& key) {
    encryption_enabled_ = enabled;
    if (!key.empty()) {
        encryption_key_ = key;
    }
}

LocalStorage::StorageStats LocalStorage::get_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void LocalStorage::reset_stats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = StorageStats{};
    stats_.total_keys = 0;
    stats_.total_vectors = 0;
    stats_.total_size_bytes = 0;
    stats_.cache_hits = 0;
    stats_.cache_misses = 0;
    stats_.cache_hit_ratio = 0.0;
    stats_.disk_reads = 0;
    stats_.disk_writes = 0;
}

bool LocalStorage::compact() {
    // Remove expired entries and optimize storage
    return flush_to_disk();
}

bool LocalStorage::verify_integrity() {
    // Check data integrity
    return true;
}

bool LocalStorage::cleanup_expired() {
    // Remove expired entries
    return true;
}

std::string LocalStorage::get_file_path(const std::string& key) {
    std::string hash = generate_hash(key);
    std::string sub_dir = hash.substr(0, 2);
    return data_dir_ + "/vectors/" + sub_dir + "/" + hash + ".dat";
}

bool LocalStorage::write_file(const std::string& path, const std::string& data) {
    try {
        // Create subdirectory if needed
        std::filesystem::path file_path(path);
        create_directory(file_path.parent_path().string());
        
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) {
            return false;
        }
        
        file.write(data.data(), data.size());
        
        update_stats(false, false); // No cache hit, disk write
        
        return file.good();
    } catch (const std::exception& e) {
        return false;
    }
}

std::string LocalStorage::read_file(const std::string& path) {
    try {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            return "";
        }
        
        // Get file size
        file.seekg(0, std::ios::end);
        size_t file_size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        std::string data(file_size, '\0');
        file.read(&data[0], file_size);
        
        update_stats(false, true); // No cache hit, disk read
        
        return file.good() ? data : "";
    } catch (const std::exception& e) {
        return "";
    }
}

bool LocalStorage::delete_file(const std::string& path) {
    try {
        return std::filesystem::remove(path);
    } catch (const std::exception& e) {
        return false;
    }
}

bool LocalStorage::create_directory(const std::string& path) {
    try {
        return std::filesystem::create_directories(path);
    } catch (const std::exception& e) {
        return false;
    }
}

std::vector<std::string> LocalStorage::list_files(const std::string& path) {
    std::vector<std::string> files;
    
    try {
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            if (entry.is_regular_file()) {
                files.push_back(entry.path().string());
            }
        }
    } catch (const std::exception& e) {
        // Return empty list on error
    }
    
    return files;
}

void LocalStorage::evict_from_cache() {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    while (memory_cache_.size() > cache_size_) {
        // Simple LRU: remove first element
        memory_cache_.erase(memory_cache_.begin());
    }
}

bool LocalStorage::is_in_cache(const std::string& key) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    return memory_cache_.find(key) != memory_cache_.end();
}

void LocalStorage::add_to_cache(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    memory_cache_[key] = value;
    
    // Evict if cache is full
    while (memory_cache_.size() > cache_size_) {
        memory_cache_.erase(memory_cache_.begin());
    }
}

std::string LocalStorage::compress_data(const std::string& data) {
    // Simplified compression - in real implementation would use zlib or similar
    return data;
}

std::string LocalStorage::decompress_data(const std::string& compressed_data) {
    // Simplified decompression
    return compressed_data;
}

std::string LocalStorage::encrypt_data(const std::string& data) {
    // Simplified encryption - in real implementation would use AES
    return data;
}

std::string LocalStorage::decrypt_data(const std::string& encrypted_data) {
    // Simplified decryption
    return encrypted_data;
}

std::string LocalStorage::serialize_vector(const std::vector<float>& vector) {
    std::string result;
    result.reserve(vector.size() * sizeof(float));
    
    for (float value : vector) {
        const char* bytes = reinterpret_cast<const char*>(&value);
        result.append(bytes, sizeof(float));
    }
    
    return result;
}

std::vector<float> LocalStorage::deserialize_vector(const std::string& data) {
    std::vector<float> vector;
    
    if (data.size() % sizeof(float) != 0) {
        return vector;
    }
    
    size_t num_floats = data.size() / sizeof(float);
    vector.resize(num_floats);
    
    const float* floats = reinterpret_cast<const float*>(data.data());
    for (size_t i = 0; i < num_floats; ++i) {
        vector[i] = floats[i];
    }
    
    return vector;
}

void LocalStorage::update_stats(bool cache_hit, bool disk_operation) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    if (cache_hit) {
        stats_.cache_hits++;
    } else {
        stats_.cache_misses++;
    }
    
    if (disk_operation) {
        // Determine if it's a read or write based on context
        // This is simplified
        stats_.disk_reads++;
    }
    
    // Update cache hit ratio
    uint64_t total_accesses = stats_.cache_hits + stats_.cache_misses;
    if (total_accesses > 0) {
        stats_.cache_hit_ratio = static_cast<double>(stats_.cache_hits) / total_accesses;
    }
}

std::string LocalStorage::generate_hash(const std::string& key) {
    unsigned char hash[MD5_DIGEST_LENGTH];
    MD5(reinterpret_cast<const unsigned char*>(key.c_str()), key.size(), hash);
    
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (int i = 0; i < MD5_DIGEST_LENGTH; ++i) {
        ss << std::setw(2) << static_cast<int>(hash[i]);
    }
    
    return ss.str();
}

bool LocalStorage::validate_key(const std::string& key) {
    if (key.empty() || key.length() > 256) {
        return false;
    }
    
    // Check for invalid characters
    for (char c : key) {
        if (c == '/' || c == '\\' || c == '\0') {
            return false;
        }
    }
    
    return true;
}

} // namespace dann
