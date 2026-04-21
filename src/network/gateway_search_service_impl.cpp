#include "gateway_search_service_impl.h"
#include "dann/logger.h"
#include <algorithm>
#include <chrono>
#include <future>

namespace dann {

GatewaySearchServiceImpl::GatewaySearchServiceImpl(int shard_count, int dimension)
    : shard_count_(shard_count), dimension_(dimension) {}

void GatewaySearchServiceImpl::add_shard(int shard_id, const std::string& address, int port) {
    std::lock_guard<std::mutex> lock(shards_mutex_);
    ShardInfo info;
    info.shard_id = shard_id;
    info.address = address;
    info.port = port;
    info.client = std::make_unique<RPCClient>(address, port);
    shards_.push_back(std::move(info));
}

bool GatewaySearchServiceImpl::connect_all_shards() {
    std::lock_guard<std::mutex> lock(shards_mutex_);
    bool all_connected = true;
    for (auto& shard : shards_) {
        if (!shard.client->connect()) {
            Logger::instance().errorf("Failed to connect to shard {} at {}:{}",
                shard.shard_id, shard.address, shard.port);
            all_connected = false;
        } else {
            Logger::instance().infof("Connected to shard {} at {}:{}",
                shard.shard_id, shard.address, shard.port);
        }
    }
    return all_connected;
}

int GatewaySearchServiceImpl::shard_id_for_document(int64_t id) const {
    const uint64_t h = std::hash<int64_t>{}(id);
    return static_cast<int>(h % static_cast<uint64_t>(shard_count_));
}

grpc::Status GatewaySearchServiceImpl::Search(grpc::ServerContext* context,
                                              const SearchRequest* request,
                                              SearchResponse* response) {
    try {
        auto start_time = std::chrono::high_resolution_clock::now();

        std::lock_guard<std::mutex> lock(shards_mutex_);

        std::vector<SearchResponse> shard_responses(shards_.size());
        std::vector<std::future<grpc::Status>> futures;
        futures.reserve(shards_.size());

        for (size_t i = 0; i < shards_.size(); ++i) {
            futures.push_back(std::async(std::launch::async, [this, i, &request, &shard_responses]() {
                return shards_[i].client->Search(*request, &shard_responses[i]);
            }));
        }

        bool any_success = false;
        for (size_t i = 0; i < futures.size(); ++i) {
            auto status = futures[i].get();
            if (status.ok() && shard_responses[i].success()) {
                any_success = true;
            } else {
                Logger::instance().warnf("Shard {} search failed: {}",
                    shards_[i].shard_id, status.error_message());
            }
        }

        if (!any_success) {
            response->set_success(false);
            response->set_error_message("All shards failed");
            return grpc::Status(grpc::StatusCode::INTERNAL, "All shards failed");
        }

        std::vector<std::pair<int, float>> all_results;
        for (const auto& shard_resp : shard_responses) {
            if (!shard_resp.success()) continue;
            for (const auto& result : shard_resp.results()) {
                all_results.emplace_back(result.id(), result.distance());
            }
        }

        std::sort(all_results.begin(), all_results.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; });

        int k = request->k();
        if (static_cast<int>(all_results.size()) > k) {
            all_results.resize(k);
        }

        for (const auto& [id, dist] : all_results) {
            auto* result = response->add_results();
            result->set_id(id);
            result->set_distance(dist);
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        response->set_success(true);
        response->set_query_time_ms(duration.count());

        return grpc::Status::OK;
    } catch (const std::exception& e) {
        Logger::instance().errorf("Gateway Search failed: {}", e.what());
        response->set_success(false);
        response->set_error_message(e.what());
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }
}

grpc::Status GatewaySearchServiceImpl::AddVectors(grpc::ServerContext* context,
                                                   const AddVectorsRequest* request,
                                                   AddVectorsResponse* response) {
    try {
        std::lock_guard<std::mutex> lock(shards_mutex_);

        std::vector<AddVectorsRequest> shard_requests(shards_.size());
        for (size_t i = 0; i < shard_requests.size(); ++i) {
            shard_requests[i].set_batch_size(request->batch_size());
            shard_requests[i].set_overwrite_existing(request->overwrite_existing());
        }

        for (const auto& vec : request->vectors()) {
            int target_shard = shard_id_for_document(vec.id());
            *shard_requests[target_shard].add_vectors() = vec;
        }

        std::vector<AddVectorsResponse> shard_responses(shards_.size());
        std::vector<std::future<grpc::Status>> futures;
        futures.reserve(shards_.size());

        for (size_t i = 0; i < shards_.size(); ++i) {
            if (shard_requests[i].vectors_size() == 0) {
                shard_responses[i].set_success(true);
                shard_responses[i].set_added_count(0);
                continue;
            }
            futures.push_back(std::async(std::launch::async, [this, i, &shard_requests, &shard_responses]() {
                return shards_[i].client->AddVectors(shard_requests[i], &shard_responses[i]);
            }));
        }

        int64_t total_added = 0;
        bool all_success = true;
        for (auto& future : futures) {
            auto status = future.get();
            if (!status.ok()) {
                all_success = false;
            }
        }

        for (const auto& shard_resp : shard_responses) {
            total_added += shard_resp.added_count();
            if (!shard_resp.success()) {
                all_success = false;
            }
        }

        response->set_success(all_success);
        response->set_added_count(total_added);
        if (!all_success) {
            response->set_error_message("Some shards failed to add vectors");
        }

        return grpc::Status::OK;
    } catch (const std::exception& e) {
        Logger::instance().errorf("Gateway AddVectors failed: {}", e.what());
        response->set_success(false);
        response->set_error_message(e.what());
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }
}

grpc::Status GatewaySearchServiceImpl::RemoveVector(grpc::ServerContext* context,
                                                     const RemoveVectorRequest* request,
                                                     RemoveVectorResponse* response) {
    try {
        std::lock_guard<std::mutex> lock(shards_mutex_);
        int target_shard = shard_id_for_document(request->id());

        if (target_shard < 0 || target_shard >= static_cast<int>(shards_.size())) {
            response->set_success(false);
            response->set_error_message("Invalid shard for vector ID");
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid shard");
        }

        return shards_[target_shard].client->RemoveVector(*request, response);
    } catch (const std::exception& e) {
        Logger::instance().errorf("Gateway RemoveVector failed: {}", e.what());
        response->set_success(false);
        response->set_error_message(e.what());
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }
}

grpc::Status GatewaySearchServiceImpl::UpdateVector(grpc::ServerContext* context,
                                                     const UpdateVectorRequest* request,
                                                     UpdateVectorResponse* response) {
    try {
        std::lock_guard<std::mutex> lock(shards_mutex_);
        int target_shard = shard_id_for_document(request->id());

        if (target_shard < 0 || target_shard >= static_cast<int>(shards_.size())) {
            response->set_success(false);
            response->set_error_message("Invalid shard for vector ID");
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid shard");
        }

        return shards_[target_shard].client->UpdateVector(*request, response);
    } catch (const std::exception& e) {
        Logger::instance().errorf("Gateway UpdateVector failed: {}", e.what());
        response->set_success(false);
        response->set_error_message(e.what());
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }
}

grpc::Status GatewaySearchServiceImpl::GetVector(grpc::ServerContext* context,
                                                  const GetVectorRequest* request,
                                                  GetVectorResponse* response) {
    try {
        std::lock_guard<std::mutex> lock(shards_mutex_);
        int target_shard = shard_id_for_document(request->id());

        if (target_shard < 0 || target_shard >= static_cast<int>(shards_.size())) {
            response->set_success(false);
            response->set_error_message("Invalid shard for vector ID");
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid shard");
        }

        std::vector<GetVectorResponse> responses(shards_.size());
        for (size_t i = 0; i < shards_.size(); ++i) {
            auto status = shards_[i].client->GetVector(*request, &responses[i]);
            if (status.ok() && responses[i].success()) {
                *response = responses[i];
                return grpc::Status::OK;
            }
        }

        response->set_success(false);
        response->set_error_message("Vector not found in any shard");
        return grpc::Status(grpc::StatusCode::NOT_FOUND, "Vector not found");
    } catch (const std::exception& e) {
        Logger::instance().errorf("Gateway GetVector failed: {}", e.what());
        response->set_success(false);
        response->set_error_message(e.what());
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }
}

grpc::Status GatewaySearchServiceImpl::GetStats(grpc::ServerContext* context,
                                                 const StatsRequest* request,
                                                 StatsResponse* response) {
    try {
        std::lock_guard<std::mutex> lock(shards_mutex_);

        int64_t total_vectors = 0;
        double total_avg_query = 0.0;
        int64_t total_queries = 0;
        int success_count = 0;

        for (auto& shard : shards_) {
            StatsResponse shard_resp;
            auto status = shard.client->GetStats(*request, &shard_resp);
            if (status.ok() && shard_resp.success()) {
                total_vectors += shard_resp.total_vectors();
                total_avg_query += shard_resp.avg_query_time_ms();
                total_queries += shard_resp.total_queries();
                success_count++;
                if (response->index_type().empty()) {
                    response->set_index_type(shard_resp.index_type());
                }
                response->set_dimension(shard_resp.dimension());
            }
        }

        response->set_success(success_count > 0);
        response->set_total_vectors(total_vectors);
        if (success_count > 0) {
            response->set_avg_query_time_ms(total_avg_query / success_count);
        }
        response->set_total_queries(total_queries);

        return grpc::Status::OK;
    } catch (const std::exception& e) {
        Logger::instance().errorf("Gateway GetStats failed: {}", e.what());
        response->set_success(false);
        response->set_error_message(e.what());
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }
}

grpc::Status GatewaySearchServiceImpl::HealthCheck(grpc::ServerContext* context,
                                                    const HealthCheckRequest* request,
                                                    HealthCheckResponse* response) {
    try {
        std::lock_guard<std::mutex> lock(shards_mutex_);

        bool all_healthy = true;
        int healthy_count = 0;

        for (auto& shard : shards_) {
            HealthCheckResponse shard_resp;
            auto status = shard.client->HealthCheck(*request, &shard_resp);
            if (status.ok() && shard_resp.healthy()) {
                healthy_count++;
            } else {
                all_healthy = false;
            }
        }

        response->set_healthy(all_healthy);
        response->set_status(all_healthy ? "healthy" : "degraded");
        response->set_version("1.0.0");

        auto& details = *response->mutable_details();
        details["total_shards"] = std::to_string(shards_.size());
        details["healthy_shards"] = std::to_string(healthy_count);

        return grpc::Status::OK;
    } catch (const std::exception& e) {
        response->set_healthy(false);
        response->set_status("unhealthy");
        response->set_error_message(e.what());
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }
}

} // namespace dann
