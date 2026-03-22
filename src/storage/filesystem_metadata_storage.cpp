#include "dann/filesystem_metadata_storage.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <iostream>

namespace dann {

FileSystemMetaDataStorage::FileSystemMetaDataStorage(const std::string& data_dir)
    : data_dir_(data_dir) {
    std::filesystem::create_directories(data_dir_);
}

std::string FileSystemMetaDataStorage::GetFilePath(const std::string& index) const {
    return data_dir_ + "/" + index;
}

void FileSystemMetaDataStorage::LoadFromFile(const std::string& index) {
    std::string file_path = GetFilePath(index);
    std::ifstream file(file_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open metadata file: " + file_path);
    }

    nlohmann::json j;
    file >> j;
    file.close();

    metadata_cache_[index] = j.get<IndexMetaData>();
}

void FileSystemMetaDataStorage::SaveToFile(const std::string& index, const IndexMetaData& metadata) {
    std::string file_path = GetFilePath(index);
    nlohmann::json j = metadata;
    
    std::ofstream file(file_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open metadata file for writing: " + file_path);
    }
    
    file << j.dump(4);
    file.close();
}

IndexMetaData& FileSystemMetaDataStorage::Get(const std::string& index) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = metadata_cache_.find(index);
    if (it == metadata_cache_.end()) {
        LoadFromFile(index);
    }
    
    return metadata_cache_[index];
}

void FileSystemMetaDataStorage::Put(const std::string& index, const IndexMetaData& metadata) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    metadata_cache_[index] = metadata;
    SaveToFile(index, metadata);
}

}
