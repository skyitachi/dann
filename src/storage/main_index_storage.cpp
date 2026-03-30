#include "dann/main_index_storage.h"
#include <fstream>
#include <cstring>
#include <algorithm>
#include <stdexcept>

namespace dann {

MainIndexStorage::MainIndexStorage() : dimension_(0), total_centroids_(0) {}

MainIndexStorage::MainIndexStorage(uint32_t dimension) : dimension_(dimension), total_centroids_(0) {}

void MainIndexStorage::AddCentroid(const float* data) {
    for (uint32_t i = 0; i < dimension_; ++i) {
        centroids_.push_back(data[i]);
    }
    ++total_centroids_;
}

void MainIndexStorage::AddAssignment(uint32_t centroid_start, uint32_t centroid_end, const std::string& node_id) {
    uint32_t node_id_index = GetOrCreateNodeIdIndex(node_id);
    assigns_.push_back({centroid_start, centroid_end, node_id_index});
}

uint32_t MainIndexStorage::GetOrCreateNodeIdIndex(const std::string& node_id) {
    auto it = node_id_to_index_.find(node_id);
    if (it != node_id_to_index_.end()) {
        return it->second;
    }
    uint32_t index = static_cast<uint32_t>(unique_node_ids_.size());
    unique_node_ids_.push_back(node_id);
    node_id_to_index_[node_id] = index;
    return index;
}

uint32_t MainIndexStorage::CalculateChecksum(const uint8_t* data, size_t size) {
    uint32_t checksum = 0;
    for (size_t i = 0; i < size; ++i) {
        checksum += static_cast<uint32_t>(data[i]);
        checksum ^= (checksum << 13);
        checksum ^= (checksum >> 17);
        checksum ^= (checksum << 5);
    }
    return checksum;
}

void MainIndexStorage::Clear() {
    total_centroids_ = 0;
    centroids_.clear();
    assigns_.clear();
    unique_node_ids_.clear();
    node_id_to_index_.clear();
}

Status MainIndexStorage::Save(const std::string& file_path) const {
    std::ofstream ofs(file_path, std::ios::binary);
    if (!ofs.is_open()) {
        return Status::IOError("Failed to open file for writing: " + file_path);
    }

    uint32_t magic = MAIN_INDEX_MAGIC;
    uint32_t version = MAIN_INDEX_VERSION;
    uint32_t num_centroids = total_centroids_;
    uint32_t num_assigns = static_cast<uint32_t>(assigns_.size());
    uint32_t num_node_ids = static_cast<uint32_t>(unique_node_ids_.size());

    ofs.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    ofs.write(reinterpret_cast<const char*>(&version), sizeof(version));
    ofs.write(reinterpret_cast<const char*>(&dimension_), sizeof(dimension_));
    ofs.write(reinterpret_cast<const char*>(&num_centroids), sizeof(num_centroids));
    ofs.write(reinterpret_cast<const char*>(&num_assigns), sizeof(num_assigns));
    ofs.write(reinterpret_cast<const char*>(&num_node_ids), sizeof(num_node_ids));

    if (dimension_ > 0 && num_centroids > 0) {
        ofs.write(reinterpret_cast<const char*>(centroids_.data()),
                  static_cast<std::streamsize>(num_centroids * dimension_ * sizeof(float)));
    }

    for (const auto& assign : assigns_) {
        ofs.write(reinterpret_cast<const char*>(&assign.centroid_start), sizeof(assign.centroid_start));
        ofs.write(reinterpret_cast<const char*>(&assign.centroid_end), sizeof(assign.centroid_end));
        ofs.write(reinterpret_cast<const char*>(&assign.node_id_index), sizeof(assign.node_id_index));
    }

    for (const auto& node_id : unique_node_ids_) {
        uint32_t len = static_cast<uint32_t>(node_id.size());
        ofs.write(reinterpret_cast<const char*>(&len), sizeof(len));
        ofs.write(node_id.data(), static_cast<std::streamsize>(len));
    }

    uint32_t footer_magic = MAIN_INDEX_FOOTER_MAGIC;
    ofs.write(reinterpret_cast<const char*>(&footer_magic), sizeof(footer_magic));

    ofs.close();
    return Status::OK();
}

Status MainIndexStorage::Load(const std::string& file_path) {
    std::ifstream ifs(file_path, std::ios::binary);
    if (!ifs.is_open()) {
        return Status::IOError("Failed to open file for reading: " + file_path);
    }

    Clear();

    uint32_t magic, version, dim, num_centroids, num_assigns, num_node_ids;
    ifs.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    ifs.read(reinterpret_cast<char*>(&version), sizeof(version));
    ifs.read(reinterpret_cast<char*>(&dim), sizeof(dim));
    ifs.read(reinterpret_cast<char*>(&num_centroids), sizeof(num_centroids));
    ifs.read(reinterpret_cast<char*>(&num_assigns), sizeof(num_assigns));
    ifs.read(reinterpret_cast<char*>(&num_node_ids), sizeof(num_node_ids));

    if (magic != MAIN_INDEX_MAGIC) {
        return Status::Corruption("Invalid magic number in file: " + file_path);
    }
    if (version != MAIN_INDEX_VERSION) {
        return Status::Corruption("Unsupported version in file: " + file_path);
    }

    dimension_ = dim;
    total_centroids_ = num_centroids;

    if (dimension_ > 0 && num_centroids > 0) {
        centroids_.resize(static_cast<size_t>(num_centroids) * static_cast<size_t>(dimension_));
        ifs.read(reinterpret_cast<char*>(centroids_.data()),
                 static_cast<std::streamsize>(centroids_.size() * sizeof(float)));
    }

    assigns_.resize(num_assigns);
    for (uint32_t i = 0; i < num_assigns; ++i) {
        ifs.read(reinterpret_cast<char*>(&assigns_[i].centroid_start), sizeof(assigns_[i].centroid_start));
        ifs.read(reinterpret_cast<char*>(&assigns_[i].centroid_end), sizeof(assigns_[i].centroid_end));
        ifs.read(reinterpret_cast<char*>(&assigns_[i].node_id_index), sizeof(assigns_[i].node_id_index));
    }

    unique_node_ids_.resize(num_node_ids);
    for (uint32_t i = 0; i < num_node_ids; ++i) {
        uint32_t len;
        ifs.read(reinterpret_cast<char*>(&len), sizeof(len));
        unique_node_ids_[i].resize(len);
        ifs.read(&unique_node_ids_[i][0], static_cast<std::streamsize>(len));
        node_id_to_index_[unique_node_ids_[i]] = i;
    }

    uint32_t footer_magic;
    ifs.read(reinterpret_cast<char*>(&footer_magic), sizeof(footer_magic));
    if (footer_magic != MAIN_INDEX_FOOTER_MAGIC) {
        return Status::Corruption("Invalid footer magic number in file: " + file_path);
    }

    ifs.close();
    return Status::OK();
}

}