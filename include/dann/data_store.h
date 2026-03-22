#pragma once

#include <string>
#include <cstddef>
#include "dann/status.h"

namespace dann {

class DataStore {
public:
    virtual ~DataStore() = default;

    virtual Status Write(const std::string& path, const void* data, size_t size, size_t* bytes_written) = 0;
    virtual Status Read(const std::string& path, void* buffer, size_t size, size_t* bytes_read) = 0;
    virtual Status ReadAt(const std::string& path, size_t offset, void* buffer, size_t size, size_t* bytes_read) = 0;
    virtual Status Exists(const std::string& path, bool* exists) = 0;
    virtual Status Delete(const std::string& path, bool* deleted) = 0;
};

}
