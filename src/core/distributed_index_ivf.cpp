//
// Created by skyitachi on 2026/2/27.
//

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <numeric>
#include <random>
#include <unistd.h>

#include "dann/distributed_index_ivf.h"

#include "dann/logger.h"
#include "dann/utils.h"
#include "dann/io_thread_pool.h"
#include "dann/status.h"

namespace dann {
    int64_t get_nlist(int64_t N) {
        int64_t nlist = N;
        if (N < 1000000) {
            nlist = 8 * sqrt(N);
        } else if (N < 10000000) {
            nlist = 65536; // 2^16
        } else if (N < 100000000) {
            nlist = 262144; // 2^18
        } else if (N < 1000000000) {
            nlist = 1048576; // 2^20
        }
        return nlist;
    }

    // 推荐的nprobe设置策略
    int determine_nprobe(int nlist, float recall_target) {
        if (recall_target >= 0.95) {
            return std::min(nlist / 4, 256); // 高召回率
        } else if (recall_target >= 0.90) {
            return std::min(nlist / 8, 128); // 中等召回率
        } else {
            return std::min(nlist / 16, 64); // 平衡性能
        }
    }

    DistributedIndexIVF::DistributedIndexIVF(std::string name, int d, int shards,
                                             std::vector<std::string> nodes): name_(std::move(name)), dimension_(d),
                                                                              shard_counts_(shards),
                                                                              nodes_(std::move(nodes)),
                                                                              is_trained_(false) {
        assert(shard_counts_ >= nodes.size() && shard_counts_ > 0);
        int node_size = nodes_.size();
        for (int i = 0; i < shard_counts_; i++) {
            shards_[i] = std::make_unique<IndexIVFShard>(d, i, nodes_[i % node_size]);
        }
    }

    DistributedIndexIVF::DistributedIndexIVF(std::string name, int d, int total_shards, int shard_id, std::string node_id):
        name_(std::move(name)), dimension_(d), shard_counts_(total_shards), 
        current_shard_id_(shard_id), is_shard_mode_(true), is_trained_(false) {
        nodes_.push_back(node_id);
        shards_[shard_id] = std::make_unique<IndexIVFShard>(d, shard_id, node_id);
    }

    DistributedIndexIVF::DistributedIndexIVF(std::string name, int d, int shards, int nlist, int nprobe,
                                         std::vector<std::string> nodes): name_(std::move(name)), dimension_(d),
                                                                          shard_counts_(shards),
                                                                          nlist_(nlist),
                                                                          nprobe_(nprobe),
                                                                          nodes_(std::move(nodes)),
                                                                          is_trained_(false) {
        assert(shard_counts_ >= nodes.size() && shard_counts_ > 0);
        // 将shards均分到nodes上
        int node_size = nodes_.size();
        for (int i = 0; i < shard_counts_; i++) {
            shards_[i] = std::make_unique<IndexIVFShard>(d, i, nodes_[i % node_size]);
        }
    }


    DistributedIndexIVF::DistributedIndexIVF(std::string name, std::string node_id, std::shared_ptr<MetaDataStorage> meta_data_storage):
        name_(std::move(name)), meta_data_storage_(meta_data_storage) {
        Status s = meta_data_storage_->Get(name_, &meta_data_);
        if (!s.ok()) {
            LOG_ERRORF("Failed to load metadata: %s", s.ToString().c_str());
            exit(1);
        }
        if (!meta_data_->routing_table.contains(node_id)) {
            LOG_ERRORF("Node %s does not exist", node_id.c_str());
            exit(1);
        }
        dimension_ = meta_data_->dimension;
        nprobe_ = meta_data_->nprobe;
    }

    int DistributedIndexIVF::dimension() const {
        return dimension_;
    }

    std::string DistributedIndexIVF::index_type() const {
        return "IVF";
    }

    void DistributedIndexIVF::build_index(const std::vector<float> &vectors,
                                          const std::vector<int64_t> &ids) {
        assert(dimension_ != 0);
        assert(vectors.size() / dimension_ == ids.size());

        const int64_t num_vectors = static_cast<int64_t>(ids.size());
        if (nlist_ < 0) {
            nlist_ = get_nlist(num_vectors);
        }
        clustering_ = std::make_unique<Clustering>(dimension_, nlist_);
        nprobe_ = determine_nprobe(nlist_, 0.90f);
        LOG_INFOF("clustering->k=%d, nprobe=%d", clustering_->k, nprobe_);

        if (num_vectors == 0) {
            global_centroids_.clear();
            global_centroid_ids_.clear();
            is_trained_ = false;
            return;
        }

        // 1) Sampling + clustering training
        const int64_t n_train = std::min(static_cast<int64_t>(clustering_->k) * 64, num_vectors);
        std::vector<float> train_vectors = sample_training_vectors(vectors, n_train);
        const int64_t actual_n_train = static_cast<int64_t>(train_vectors.size() / dimension_);
        LOG_INFOF("clustering->k=%d, nprobe=%d, ntrain=%ld, actual_n_train=%ld", clustering_->k, nprobe_, n_train, actual_n_train);

        clustering_->train(train_vectors, actual_n_train);
        global_centroids_ = clustering_->centroids;
        const int64_t num_centroids = static_cast<int64_t>(global_centroids_.size() / dimension_);

        global_centroid_ids_.resize(num_centroids);
        std::iota(global_centroid_ids_.begin(), global_centroid_ids_.end(), 0);

        // 2) First pass: count vectors per centroid to reserve exact capacity
        std::vector<int64_t> centroid_counts(num_centroids, 0);
        std::vector<int64_t> assignments(num_vectors, 0);
        for (int64_t i = 0; i < num_vectors; ++i) {
            const int64_t centroid = find_closest_optimized(
                global_centroids_.data(),
                vectors.data() + i * dimension_,
                dimension_,
                static_cast<int>(num_centroids));
            assignments[i] = centroid;
            ++centroid_counts[centroid];
        }

        // 3) Build postings using pre-sized vectors to reduce reallocation/copies
        std::vector<InvertedList> postings(num_centroids);
        for (int64_t centroid = 0; centroid < num_centroids; ++centroid) {
            const auto c = static_cast<size_t>(centroid_counts[centroid]);
            postings[centroid].vectors.resize(c * static_cast<size_t>(dimension_));
            postings[centroid].vector_ids.resize(c);
        }

        std::vector<int64_t> cursor(num_centroids, 0);
        for (int64_t i = 0; i < num_vectors; ++i) {
            const int64_t centroid = assignments[i];
            const int64_t pos = cursor[centroid]++;

            float *dst = postings[centroid].vectors.data() + pos * dimension_;
            const float *src = vectors.data() + i * dimension_;
            std::copy(src, src + dimension_, dst);
            postings[centroid].vector_ids[static_cast<size_t>(pos)] = ids[static_cast<size_t>(i)];
        }

        // 4) Distribute postings to shards
        for (int64_t centroid = 0; centroid < num_centroids; ++centroid) {
            if (postings[centroid].vector_ids.empty()) {
                continue;
            }
            const int shard_id = static_cast<int>(centroid % shard_counts_);
            shards_[shard_id]->add_posting(static_cast<int>(centroid), std::move(postings[centroid]));
        }
        
        // 5) Build main index storage with centroids and assignments
        main_index_storage_ = std::make_unique<MainIndexStorage>(dimension_);
        for (int64_t centroid = 0; centroid < num_centroids; ++centroid) {
            main_index_storage_->AddCentroid(global_centroids_.data() + centroid * dimension_);
        }
        
        // Build assignments map: for each shard, track which centroids belong to it
        std::unordered_map<int, std::vector<uint32_t>> shard_to_centroids;
        for (int64_t centroid = 0; centroid < num_centroids; ++centroid) {
            if (!postings[centroid].vector_ids.empty()) {
                int shard_id = static_cast<int>(centroid % shard_counts_);
                shard_to_centroids[shard_id].push_back(static_cast<uint32_t>(centroid));
            }
        }
        
        // Add assignments for each shard
        for (const auto& [shard_id, centroid_list] : shard_to_centroids) {
            if (centroid_list.empty()) continue;
            
            uint32_t centroid_start = centroid_list.front();
            uint32_t centroid_end = centroid_list.back();
            std::string node_id = nodes_[shard_id % nodes_.size()];
            
            main_index_storage_->AddAssignment(centroid_start, centroid_end, node_id);
        }

        is_trained_ = true;
    }

    std::vector<InternalSearchResult> DistributedIndexIVF::search(const std::vector<float> &query, int k) {
        int nprobe = nprobe_;

        if (nprobe > global_centroid_ids_.size()) {
            nprobe = global_centroid_ids_.size();
        }
        // 从global_vectors中找到nprobe和query最近的向量
        std::vector<DistanceWithIndex> closest_centroids =
                find_closest_k_with_distance(&global_centroids_[0], &query[0], dimension_, global_centroid_ids_.size(),
                                             nprobe);

        std::vector<InternalSearchResult> results;
        std::unordered_map<int, std::vector<int64_t> > query_centroids_map;
        for (const auto &centroid: closest_centroids) {
            int shard_id = global_centroid_ids_[centroid.index] % shard_counts_;
            query_centroids_map[shard_id].push_back(global_centroid_ids_[centroid.index]);
        }

        // Parallel search using IO thread pool
        auto& thread_pool = get_io_thread_pool();
        std::vector<std::future<std::vector<InternalSearchResult>>> futures;
        futures.reserve(query_centroids_map.size());

        for (const auto &[shard_id, centroids]: query_centroids_map) {
            futures.push_back(thread_pool.enqueue([this, shard_id, &centroids, &query, k]() {
                return shards_[shard_id]->search(centroids, query, k);
            }));
        }

        // Collect results from all futures
        for (auto& fut : futures) {
            auto shard_result = fut.get();
            results.insert(results.end(), shard_result.begin(), shard_result.end());
        }

        std::sort(results.begin(), results.end());
        results.resize(k);
        return results;
    }

    std::vector<float> DistributedIndexIVF::sample_training_vectors(const std::vector<float> &vectors,
                                                                    int64_t n_train) const {
        const int64_t total_vectors = vectors.size() / dimension_;
        const int64_t actual_n_train = std::min(n_train, total_vectors);

        // Pre-allocate result vector
        std::vector<float> train_vectors;
        train_vectors.reserve(actual_n_train * dimension_);

        // Use reservoir sampling for better performance on large datasets
        std::random_device rd;
        std::mt19937 gen(rd());

        if (actual_n_train >= total_vectors) {
            // If we need all vectors, just copy them
            train_vectors = vectors;
        } else {
            // Reservoir sampling algorithm - O(N) time, O(K) space
            std::vector<int64_t> reservoir(actual_n_train);

            // Initialize reservoir with first K elements
            for (int64_t i = 0; i < actual_n_train; ++i) {
                reservoir[i] = i;
            }

            // Replace elements with decreasing probability
            for (int64_t i = actual_n_train; i < total_vectors; ++i) {
                std::uniform_int_distribution<int64_t> dist(0, i);
                int64_t j = dist(gen);
                if (j < actual_n_train) {
                    reservoir[j] = i;
                }
            }

            // Extract selected vectors
            for (int64_t idx: reservoir) {
                train_vectors.insert(train_vectors.end(),
                                     vectors.begin() + idx * dimension_,
                                     vectors.begin() + (idx + 1) * dimension_);
            }
        }

        return train_vectors;
    }

    int64_t DistributedIndexIVF::find_closest_optimized(const float *x, const float *y, int d, int n) const {
        float min_dis = std::numeric_limits<float>::max();
        int64_t r = 0;

        // Unroll loop for better performance when d is small and known
        if (d == 4) {
            for (int64_t i = 0; i < n; i++) {
                const float *centroid = x + i * d;
                float dx = centroid[0] - y[0];
                float dy = centroid[1] - y[1];
                float dz = centroid[2] - y[2];
                float dw = centroid[3] - y[3];
                float dis = dx * dx + dy * dy + dz * dz + dw * dw;

                if (dis < min_dis) {
                    min_dis = dis;
                    r = i;
                }
            }
        } else if (d == 8) {
            for (int64_t i = 0; i < n; i++) {
                const float *centroid = x + i * d;
                float dis = 0.0f;
                for (int j = 0; j < 8; j += 4) {
                    float dx = centroid[j] - y[j];
                    float dy = centroid[j + 1] - y[j + 1];
                    float dz = centroid[j + 2] - y[j + 2];
                    float dw = centroid[j + 3] - y[j + 3];
                    dis += dx * dx + dy * dy + dz * dz + dw * dw;
                }

                if (dis < min_dis) {
                    min_dis = dis;
                    r = i;
                }
            }
        } else {
            // General case - use original implementation
            for (int64_t i = 0; i < n; i++) {
                float dis = L2_distance(x + i * d, y, d);
                if (dis < min_dis) {
                    min_dis = dis;
                    r = i;
                }
            }
        }

        return r;
    }

    Status DistributedIndexIVF::SaveMetadata(const std::string& base_path) const {
        IndexPersistenceMetadata meta;
        meta.index_name = name_;
        meta.dimension = dimension_;
        meta.shard_count = shard_counts_;
        meta.nlist = nlist_;
        meta.nprobe = nprobe_;
        meta.index_type = "IVF";

        nlohmann::json j = meta;
        std::string json_str = j.dump(2);

        std::string meta_path = base_path + "/" + name_ + ".meta";
        std::string tmp_path = meta_path + ".tmp";

        FILE* fp = fopen(tmp_path.c_str(), "wb");
        if (!fp) {
            return Status::IOError("Failed to open metadata file for writing: " + tmp_path);
        }

        size_t written = fwrite(json_str.data(), 1, json_str.size(), fp);
        if (written != json_str.size()) {
            fclose(fp);
            std::filesystem::remove(tmp_path);
            return Status::IOError("Failed to write metadata file: " + tmp_path);
        }

        fflush(fp);
        fsync(fileno(fp));
        fclose(fp);

        std::error_code ec;
        std::filesystem::rename(tmp_path, meta_path, ec);
        if (ec) {
            std::filesystem::remove(tmp_path);
            return Status::IOError("Failed to rename metadata file: " + tmp_path + " -> " + meta_path);
        }

        return Status::OK();
    }

    Status DistributedIndexIVF::LoadMetadata(const std::string& base_path, IndexPersistenceMetadata* meta) {
        std::string meta_path = base_path + "/" + name_ + ".meta";

        if (!std::filesystem::exists(meta_path)) {
            return Status::NotExist("Metadata file does not exist: " + meta_path);
        }

        std::ifstream ifs(meta_path);
        if (!ifs.is_open()) {
            return Status::IOError("Failed to open metadata file for reading: " + meta_path);
        }

        std::string content((std::istreambuf_iterator<char>(ifs)),
                            std::istreambuf_iterator<char>());
        ifs.close();

        try {
            nlohmann::json j = nlohmann::json::parse(content);
            *meta = j.get<IndexPersistenceMetadata>();
        } catch (const nlohmann::json::exception& e) {
            return Status::Corruption("Failed to parse metadata JSON: " + std::string(e.what()));
        }

        if (meta->dimension != dimension_) {
            return Status::Corruption("Dimension mismatch in metadata: expected " + 
                                      std::to_string(dimension_) + ", got " + 
                                      std::to_string(meta->dimension));
        }

        return Status::OK();
    }

    Status DistributedIndexIVF::SaveCentroids(const std::string& base_path) const {
        if (!main_index_storage_) {
            return Status::InvalidArgument("Main index storage not initialized");
        }

        std::string centroids_path = base_path + "/" + name_ + ".centroids";
        return main_index_storage_->Save(centroids_path);
    }

    Status DistributedIndexIVF::LoadCentroids(const std::string& base_path) {
        std::string centroids_path = base_path + "/" + name_ + ".centroids";

        if (!std::filesystem::exists(centroids_path)) {
            return Status::NotExist("Centroids file does not exist: " + centroids_path);
        }

        if (!main_index_storage_) {
            main_index_storage_ = std::make_unique<MainIndexStorage>();
        }

        Status s = main_index_storage_->Load(centroids_path);
        if (!s.ok()) {
            return s;
        }

        const auto& centroids = main_index_storage_->centroids();
        global_centroids_ = centroids;
        global_centroid_ids_.resize(main_index_storage_->total_centroids());
        std::iota(global_centroid_ids_.begin(), global_centroid_ids_.end(), 0);

        is_trained_ = true;
        return Status::OK();
    }

    Status DistributedIndexIVF::SaveShards(const std::string& base_path) const {
        std::string shards_dir = base_path + "/" + name_ + ".shards";

        std::error_code ec;
        if (!std::filesystem::exists(shards_dir)) {
            if (!std::filesystem::create_directories(shards_dir, ec)) {
                return Status::IOError("Failed to create shards directory: " + shards_dir);
            }
        }

        for (const auto& [shard_id, shard] : shards_) {
            std::string shard_path = shards_dir + "/shard_" + std::to_string(shard_id) + ".posting";
            Status s = shard->Save(shard_path);
            if (!s.ok()) {
                return s;
            }
        }

        return Status::OK();
    }

    Status DistributedIndexIVF::LoadShards(const std::string& base_path) {
        std::string shards_dir = base_path + "/" + name_ + ".shards";

        if (!std::filesystem::exists(shards_dir)) {
            return Status::NotExist("Shards directory does not exist: " + shards_dir);
        }

        for (auto& [shard_id, shard] : shards_) {
            std::string shard_path = shards_dir + "/shard_" + std::to_string(shard_id) + ".posting";

            if (!std::filesystem::exists(shard_path)) {
                continue;
            }

            Status s = shard->Load(shard_path);
            if (!s.ok()) {
                return s;
            }
        }

        return Status::OK();
    }

    bool DistributedIndexIVF::save_index(const std::string& index_path) {
        std::shared_lock<std::shared_mutex> lock(rw_mutex_);

        std::error_code ec;
        if (!std::filesystem::exists(index_path)) {
            if (!std::filesystem::create_directories(index_path, ec)) {
                LOG_ERRORF("Failed to create index directory: %s", index_path.c_str());
                return false;
            }
        }

        Status s = SaveMetadata(index_path);
        if (!s.ok()) {
            LOG_ERRORF("Failed to save metadata: %s", s.ToString().c_str());
            return false;
        }

        s = SaveCentroids(index_path);
        if (!s.ok()) {
            LOG_ERRORF("Failed to save centroids: %s", s.ToString().c_str());
            return false;
        }

        s = SaveShards(index_path);
        if (!s.ok()) {
            LOG_ERRORF("Failed to save shards: %s", s.ToString().c_str());
            return false;
        }

        dirty_.store(false);
        return true;
    }

    bool DistributedIndexIVF::load_index(const std::string& index_path) {
        std::unique_lock<std::shared_mutex> lock(rw_mutex_);

        IndexPersistenceMetadata meta;
        Status s = LoadMetadata(index_path, &meta);
        if (!s.ok()) {
            LOG_ERRORF("Failed to load metadata: %s", s.ToString().c_str());
            return false;
        }

        if (meta.shard_count != shard_counts_) {
            LOG_ERRORF("Shard count mismatch: expected %d, got %d", 
                       shard_counts_, meta.shard_count);
            return false;
        }

        nlist_ = meta.nlist;
        nprobe_ = meta.nprobe;

        s = LoadCentroids(index_path);
        if (!s.ok()) {
            LOG_ERRORF("Failed to load centroids: %s", s.ToString().c_str());
            return false;
        }

        s = LoadShards(index_path);
        if (!s.ok()) {
            LOG_ERRORF("Failed to load shards: %s", s.ToString().c_str());
            return false;
        }

        dirty_.store(false);
        return true;
    }

    bool DistributedIndexIVF::add_vectors(const std::vector<float>& vectors, const std::vector<int64_t>& ids) {
        std::unique_lock<std::shared_mutex> lock(rw_mutex_);
        build_index(vectors, ids);
        dirty_.store(true);
        return true;
    }
    
    size_t DistributedIndexIVF::size() {
        size_t total = 0;
        for (const auto& [shard_id, shard] : shards_) {
            total += shard->total_vectors();
        }
        return total;
    }
    
    bool DistributedIndexIVF::load_shard_only(const std::string& base_path, int shard_id) {
        std::unique_lock<std::shared_mutex> lock(rw_mutex_);
        
        IndexPersistenceMetadata meta;
        Status s = LoadMetadata(base_path, &meta);
        if (!s.ok()) {
            LOG_ERRORF("Failed to load metadata: %s", s.ToString().c_str());
            return false;
        }
        
        if (shard_id < 0 || shard_id >= meta.shard_count) {
            LOG_ERRORF("Invalid shard_id %d, must be in [0, %d)", shard_id, meta.shard_count);
            return false;
        }
        
        shard_counts_ = meta.shard_count;
        nlist_ = meta.nlist;
        nprobe_ = meta.nprobe;
        current_shard_id_ = shard_id;
        
        s = LoadCentroids(base_path);
        if (!s.ok()) {
            LOG_ERRORF("Failed to load centroids: %s", s.ToString().c_str());
            return false;
        }
        
        std::string shards_dir = base_path + "/" + name_ + ".shards";
        std::string shard_path = shards_dir + "/shard_" + std::to_string(shard_id) + ".posting";
        
        if (!std::filesystem::exists(shard_path)) {
            LOG_ERRORF("Shard file does not exist: %s", shard_path.c_str());
            return false;
        }
        
        if (shards_.find(shard_id) == shards_.end()) {
            shards_[shard_id] = std::make_unique<IndexIVFShard>(dimension_, shard_id, nodes_[0]);
        }
        
        s = shards_[shard_id]->Load(shard_path);
        if (!s.ok()) {
            LOG_ERRORF("Failed to load shard %d: %s", shard_id, s.ToString().c_str());
            return false;
        }
        
        dirty_.store(false);
        LOG_INFOF("Successfully loaded shard %d from %s", shard_id, shard_path.c_str());
        return true;
    }
}
