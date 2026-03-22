#include "dann/filesystem_data_store.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdexcept>
#include <filesystem>

namespace dann {

FileSystemDataStore::FileSystemDataStore(const std::string& base_dir) : base_dir_(base_dir) {
    std::filesystem::create_directories(base_dir_);
}

std::string FileSystemDataStore::GetFullPath(const std::string& path) const {
    return base_dir_ + "/" + path;
}

size_t FileSystemDataStore::Write(const std::string& path, const void* data, size_t size) {
    std::string full_path = GetFullPath(path);
    int fd = open(full_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        throw std::runtime_error("Failed to open file for writing: " + full_path);
    }
    ssize_t bytes_written = write(fd, data, size);
    close(fd);
    if (bytes_written < 0) {
        throw std::runtime_error("Failed to write file: " + full_path);
    }
    return bytes_written;
}

size_t FileSystemDataStore::Read(const std::string& path, void* buffer, size_t size) {
    return ReadAt(path, 0, buffer, size);
}

size_t FileSystemDataStore::ReadAt(const std::string& path, size_t offset, void* buffer, size_t size) {
    std::string full_path = GetFullPath(path);
    int fd = open(full_path.c_str(), O_RDONLY);
    if (fd < 0) {
        throw std::runtime_error("Failed to open file for reading: " + full_path);
    }
    lseek(fd, offset, SEEK_SET);
    ssize_t bytes_read = read(fd, buffer, size);
    close(fd);
    if (bytes_read < 0) {
        throw std::runtime_error("Failed to read file: " + full_path);
    }
    return bytes_read;
}

bool FileSystemDataStore::Exists(const std::string& path) {
    std::string full_path = GetFullPath(path);
    return access(full_path.c_str(), F_OK) == 0;
}

bool FileSystemDataStore::Delete(const std::string& path) {
    std::string full_path = GetFullPath(path);
    return unlink(full_path.c_str()) == 0;
}

}
