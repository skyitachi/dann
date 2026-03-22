#pragma once

#include <string>
#include <memory>
#include "dann/index_meta_data.h"

namespace dann {

class MetaDataStorage {
public:
    virtual ~MetaDataStorage() = default;

    virtual IndexMetaData& Get(const std::string& index) = 0;
    virtual void Put(const std::string& index, const IndexMetaData& metadata) = 0;
};

}
