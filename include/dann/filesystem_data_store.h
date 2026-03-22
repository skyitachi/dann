#pragma once

#include "dann/data_store.h"
#include <string>
#include <mutex>
#include <filesystem>

namespace dann {

class FileSystemDataStore : public DataStore {
public:
    explicit FileSystemDataStore(const std::string& base_dir);

    Status Write(const std::string& path, const void* data, size_t size, size_t* bytes_written) override;
    Status Read(const std::string& path, void* buffer, size_t size, size_t* bytes_read) override;
    Status ReadAt(const std::string& path, size_t offset, void* buffer, size_t size, size_t* bytes_read) override;
    Status Exists(const std::string& path, bool* exists) override;
    Status Delete(const std::string& path, bool* deleted) override;

private:
    std::string GetFullPath(const std::string& path) const;

    std::string base_dir_;
    mutable std::mutex mutex_;
};

}
