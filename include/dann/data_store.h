#pragma once

#include <string>
#include <cstddef>

namespace dann {

class DataStore {
public:
    virtual ~DataStore() = default;

    virtual size_t Write(const std::string& path, const void* data, size_t size) = 0;
    virtual size_t Read(const std::string& path, void* buffer, size_t size) = 0;
    virtual size_t ReadAt(const std::string& path, size_t offset, void* buffer, size_t size) = 0;
    virtual bool Exists(const std::string& path) = 0;
    virtual bool Delete(const std::string& path) = 0;
};

}
