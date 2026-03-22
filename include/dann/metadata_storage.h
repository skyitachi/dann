#pragma once

#include <string>
#include <memory>
#include "dann/index_meta_data.h"
#include "dann/status.h"

namespace dann {

class MetaDataStorage {
public:
    virtual ~MetaDataStorage() = default;

    virtual Status Get(const std::string& index, std::shared_ptr<IndexMetaData>* metadata) = 0;
    virtual Status Put(const std::string& index, const IndexMetaData& metadata) = 0;
};

}
