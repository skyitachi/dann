#pragma once

#include <string>
#include <memory>
#include <fstream>
#include <mutex>
#include <unordered_map>
#include "dann/metadata_storage.h"
#include "dann/index_meta_data.h"

namespace dann {

class FileSystemMetaDataStorage : public MetaDataStorage {
public:
    explicit FileSystemMetaDataStorage(const std::string& data_dir);
    ~FileSystemMetaDataStorage() override = default;

    IndexMetaData& Get(const std::string& index) override;
    void Put(const std::string& index, const IndexMetaData& metadata) override;

private:
    std::string data_dir_;
    std::unordered_map<std::string, IndexMetaData> metadata_cache_;
    std::mutex mutex_;

    std::string GetFilePath(const std::string& index) const;
    void LoadFromFile(const std::string& index);
    void SaveToFile(const std::string& index, const IndexMetaData& metadata);
};

}
