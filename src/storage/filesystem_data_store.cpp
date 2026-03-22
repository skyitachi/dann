#include "dann/filesystem_data_store.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdexcept>
#include <filesystem>
#include <cstring>

namespace dann {

FileSystemDataStore::FileSystemDataStore(const std::string& base_dir) : base_dir_(base_dir) {
    std::filesystem::create_directories(base_dir_);
}

std::string FileSystemDataStore::GetFullPath(const std::string& path) const {
    return base_dir_ + "/" + path;
}

Status FileSystemDataStore::Write(const std::string& path, const void* data, size_t size, size_t* bytes_written) {
    std::string full_path = GetFullPath(path);
    int fd = open(full_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        return Status::IOError("Failed to open file for writing: " + full_path, std::string(strerror(errno)));
    }
    ssize_t written = write(fd, data, size);
    if (written < 0) {
        close(fd);
        return Status::IOError("Failed to write file: " + full_path, std::string(strerror(errno)));
    }
    close(fd);
    *bytes_written = static_cast<size_t>(written);
    return Status::OK();
}

Status FileSystemDataStore::Read(const std::string& path, void* buffer, size_t size, size_t* bytes_read) {
    return ReadAt(path, 0, buffer, size, bytes_read);
}

Status FileSystemDataStore::ReadAt(const std::string& path, size_t offset, void* buffer, size_t size, size_t* bytes_read) {
    std::string full_path = GetFullPath(path);
    int fd = open(full_path.c_str(), O_RDONLY);
    if (fd < 0) {
        return Status::IOError("Failed to open file for reading: " + full_path, std::string(strerror(errno)));
    }
    if (lseek(fd, offset, SEEK_SET) < 0) {
        close(fd);
        return Status::IOError("Failed to seek file: " + full_path, std::string(strerror(errno)));
    }
    ssize_t read = ::read(fd, buffer, size);
    close(fd);
    if (read < 0) {
        return Status::IOError("Failed to read file: " + full_path, std::string(strerror(errno)));
    }
    *bytes_read = static_cast<size_t>(read);
    return Status::OK();
}

Status FileSystemDataStore::Exists(const std::string& path, bool* exists) {
    std::string full_path = GetFullPath(path);
    *exists = access(full_path.c_str(), F_OK) == 0;
    return Status::OK();
}

Status FileSystemDataStore::Delete(const std::string& path, bool* deleted) {
    std::string full_path = GetFullPath(path);
    int result = unlink(full_path.c_str());
    *deleted = (result == 0);
    if (result != 0 && errno != ENOENT) {
        return Status::IOError("Failed to delete file: " + full_path, std::string(strerror(errno)));
    }
    return Status::OK();
}

}
