#pragma once

#include "dann/data_store.h"
#include <string>
#include <mutex>
#include <filesystem>

namespace dann {

class FileSystemDataStore : public DataStore {
public:
    explicit FileSystemDataStore(const std::string& base_dir);

    size_t Write(const std::string& path, const void* data, size_t size) override;
    size_t Read(const std::string& path, void* buffer, size_t size) override;
    size_t ReadAt(const std::string& path, size_t offset, void* buffer, size_t size) override;
    bool Exists(const std::string& path) override;
    bool Delete(const std::string& path) override;

private:
    std::string GetFullPath(const std::string& path) const;

    std::string base_dir_;
    mutable std::mutex mutex_;
};

}
