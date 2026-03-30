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

Status FileSystemMetaDataStorage::LoadFromFile(const std::string& index) {
    std::string file_path = GetFilePath(index);
    std::ifstream file(file_path);
    if (!file.is_open()) {
        return Status::IOError("Failed to open metadata file for reading: " + file_path);
    }

    nlohmann::json j;
    try {
        file >> j;
        file.close();
        metadata_cache_[index] = j.get<IndexMetaData>();
    } catch (const std::exception& e) {
        return Status::Corruption("Failed to parse metadata file: " + file_path, e.what());
    }

    return Status::OK();
}

Status FileSystemMetaDataStorage::SaveToFile(const std::string& index, const IndexMetaData& metadata) {
    std::string file_path = GetFilePath(index);
    nlohmann::json j = metadata;

    std::ofstream file(file_path);
    if (!file.is_open()) {
        return Status::IOError("Failed to open metadata file for writing: " + file_path);
    }

    try {
        file << j.dump(4);
        file.close();
    } catch (const std::exception& e) {
        return Status::IOError("Failed to write metadata file: " + file_path, e.what());
    }

    return Status::OK();
}

Status FileSystemMetaDataStorage::Get(const std::string& index, std::shared_ptr<IndexMetaData>* metadata) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = metadata_cache_.find(index);
    if (it == metadata_cache_.end()) {
        Status s = LoadFromFile(index);
        if (!s.ok()) {
            return s;
        }
    }

    *metadata = std::make_shared<IndexMetaData>(metadata_cache_[index]);
    return Status::OK();
}

Status FileSystemMetaDataStorage::Put(const std::string& index, const IndexMetaData& metadata) {
    std::lock_guard<std::mutex> lock(mutex_);

    metadata_cache_[index] = metadata;
    return SaveToFile(index, metadata);
}

}
