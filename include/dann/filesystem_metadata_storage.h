#pragma once

#include <string>
#include <memory>
#include <fstream>
#include <mutex>
#include <unordered_map>
#include "dann/metadata_storage.h"
#include "dann/index_meta_data.h"
#include "dann/status.h"

namespace dann {

class FileSystemMetaDataStorage : public MetaDataStorage {
public:
    explicit FileSystemMetaDataStorage(const std::string& data_dir);
    ~FileSystemMetaDataStorage() override = default;

    Status Get(const std::string& index, std::shared_ptr<IndexMetaData>* metadata) override;
    Status Put(const std::string& index, const IndexMetaData& metadata) override;

private:
    std::string data_dir_;
    std::unordered_map<std::string, IndexMetaData> metadata_cache_;
    std::mutex mutex_;

    std::string GetFilePath(const std::string& index) const;
    Status LoadFromFile(const std::string& index);
    Status SaveToFile(const std::string& index, const IndexMetaData& metadata);
};

}
