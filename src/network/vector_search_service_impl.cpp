#include "vector_search_service_impl.h"
#include "dann/logger.h"
#include <chrono>

namespace dann {

VectorSearchServiceImpl::VectorSearchServiceImpl(std::shared_ptr<VectorIndex> vector_index)
    : vector_index_(std::move(vector_index)) {
    if (!vector_index_) {
        throw std::invalid_argument("VectorIndex cannot be null");
    }
}

grpc::Status VectorSearchServiceImpl::Search(grpc::ServerContext* context,
                                           const SearchRequest* request,
                                           SearchResponse* response) {
    try {
        auto start_time = std::chrono::high_resolution_clock::now();
        std::vector<float> query_vector(request->query_vector().begin(),
                                       request->query_vector().end());

        auto search_result = vector_index_->search(query_vector, request->k());
        response->set_success(true);

        // Calculate query time
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        response->set_query_time_ms(duration.count());

        // Convert search results to protobuf format
        for (const auto& result : search_result) {
            auto* proto_result = response->add_results();
            proto_result->set_id(result.id);
            proto_result->set_distance(result.distance);
            
            // Add vector data if needed
            for (float val : result.vector) {
                proto_result->add_vector(val);
            }
        }

        return grpc::Status::OK;
        
    } catch (const std::exception& e) {
        Logger::instance().errorf("Search failed: {}", e.what());
        response->set_success(false);
        response->set_error_message(e.what());
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }
}

grpc::Status VectorSearchServiceImpl::AddVectors(grpc::ServerContext* context,
                                               const dann::AddVectorsRequest* request,
                                               dann::AddVectorsResponse* response) {
    try {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Extract vectors and IDs from request
        std::vector<float> vectors;
        std::vector<int64_t> ids;
        
        for (const auto& vector : request->vectors()) {
            ids.push_back(vector.id());
            vectors.insert(vectors.end(), vector.data().begin(), vector.data().end());
        }
        
        // Use batch size from request or default
        int batch_size = request->batch_size() > 0 ? request->batch_size() : 1000;
        
        // Add vectors to index
        bool success = vector_index_->add_vectors_bulk(vectors, ids, batch_size);
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto load_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        response->set_success(success);
        response->set_added_count(success ? ids.size() : 0);
        response->set_load_time_ms(load_time.count());
        
        if (!success) {
            response->set_error_message("Failed to add vectors to index");
        }
        
        Logger::instance().infof("AddVectors completed: count={}, success={}, time_ms={}", 
                ids.size(), success, load_time.count());
        
        return grpc::Status::OK;
        
    } catch (const std::exception& e) {
        Logger::instance().errorf("AddVectors failed: {}", e.what());
        response->set_success(false);
        response->set_error_message(e.what());
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }
}

grpc::Status VectorSearchServiceImpl::RemoveVector(grpc::ServerContext* context,
                                                  const dann::RemoveVectorRequest* request,
                                                  dann::RemoveVectorResponse* response) {
    try {
        bool success = vector_index_->remove_vector(request->id());
        
        response->set_success(success);
        if (!success) {
            response->set_error_message("Failed to remove vector with ID: " + std::to_string(request->id()));
        }
        
        Logger::instance().infof("RemoveVector completed: id={}, success={}", request->id(), success);
        
        return grpc::Status::OK;
        
    } catch (const std::exception& e) {
        Logger::instance().errorf("RemoveVector failed: {}", e.what());
        response->set_success(false);
        response->set_error_message(e.what());
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }
}

grpc::Status VectorSearchServiceImpl::UpdateVector(grpc::ServerContext* context,
                                                  const dann::UpdateVectorRequest* request,
                                                  dann::UpdateVectorResponse* response) {
    try {
        std::vector<float> vector_data(request->vector().begin(), request->vector().end());
        bool success = vector_index_->update_vector(request->id(), vector_data);
        
        response->set_success(success);
        if (!success) {
            response->set_error_message("Failed to update vector with ID: " + std::to_string(request->id()));
        }
        
        Logger::instance().infof("UpdateVector completed: id={}, success={}", request->id(), success);
        
        return grpc::Status::OK;
        
    } catch (const std::exception& e) {
        Logger::instance().errorf("UpdateVector failed: {}", e.what());
        response->set_success(false);
        response->set_error_message(e.what());
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }
}

grpc::Status VectorSearchServiceImpl::GetVector(grpc::ServerContext* context,
                                              const dann::GetVectorRequest* request,
                                              dann::GetVectorResponse* response) {
    try {
        // Note: VectorIndex doesn't have a get_vector method, so this is a placeholder
        // In a real implementation, you would need to store vectors separately or extend VectorIndex
        response->set_success(false);
        response->set_error_message("GetVector not implemented - VectorIndex doesn't support vector retrieval");
        
        Logger::instance().warnf("GetVector called but not implemented for id={}", request->id());
        
        return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "GetVector not implemented");
        
    } catch (const std::exception& e) {
        Logger::instance().errorf("GetVector failed: {}", e.what());
        response->set_success(false);
        response->set_error_message(e.what());
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }
}

grpc::Status VectorSearchServiceImpl::GetStats(grpc::ServerContext* context,
                                             const dann::StatsRequest* request,
                                             dann::StatsResponse* response) {
    try {
        response->set_success(true);
        response->set_total_vectors(vector_index_->size());
        response->set_index_type(vector_index_->index_type());
        response->set_dimension(vector_index_->dimension());
        
        // Get query router metrics
        // auto query_metrics = query_router_->get_metrics();
        // response->set_avg_query_time_ms(query_metrics.avg_response_time_ms);
        // response->set_total_queries(query_metrics.total_queries);
        //
        // // Add custom metrics
        // auto& custom_metrics = *response->mutable_custom_metrics();
        // custom_metrics["index_version"] = static_cast<double>(vector_index_->get_version());
        //
        // Logger::instance().infof("GetStats completed: vectors={}, type={}, dimension={}",
        //         response->total_vectors(), response->index_type(), response->dimension());
        
        return grpc::Status::OK;
        
    } catch (const std::exception& e) {
        Logger::instance().errorf("GetStats failed: {}", e.what());
        response->set_success(false);
        response->set_error_message(e.what());
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }
}

grpc::Status VectorSearchServiceImpl::HealthCheck(grpc::ServerContext* context,
                                                  const dann::HealthCheckRequest* request,
                                                  dann::HealthCheckResponse* response) {
    try {
        response->set_healthy(true);
        response->set_status("healthy");
        response->set_version("1.0.0");
        response->set_uptime_seconds(0); // TODO: implement uptime tracking
        
        auto& details = *response->mutable_details();
        details["index_size"] = std::to_string(vector_index_->size());
        details["index_type"] = vector_index_->index_type();
        
        return grpc::Status::OK;
        
    } catch (const std::exception& e) {
        Logger::instance().errorf("HealthCheck failed: {}", e.what());
        response->set_healthy(false);
        response->set_status("unhealthy");
        response->set_error_message(e.what());
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }
}

// Helper methods implementation

// SearchResult VectorSearchServiceImpl::convert_to_search_result(const ::dann::SearchResult& proto_result) const {
//     std::vector<float> vector_data(proto_result.vector().begin(), proto_result.vector().end());
//     return SearchResult(proto_result.id(), proto_result.distance(), vector_data);
// }
//
// ::dann::SearchResult VectorSearchServiceImpl::convert_to_proto_search_result(const SearchResult& result) const {
//     ::dann::SearchResult proto_result;
//     proto_result.set_id(result.id);
//     proto_result.set_distance(result.distance);
//
//     // Note: We don't return the actual vector data as per VectorIndex::search documentation
//     // proto_result.mutable_vector()->Add(result.vector.begin(), result.vector.end());
//
//     return proto_result;
// }
//
// QueryRequest VectorSearchServiceImpl::convert_to_query_request(const ::dann::SearchRequest& proto_request) const {
//     std::vector<float> query_vector(proto_request.query_vector().begin(),
//                                    proto_request.query_vector().end());
//
//     QueryRequest request(query_vector, proto_request.k());
//
//     if (!proto_request.consistency_level().empty()) {
//         request.consistency_level = proto_request.consistency_level();
//     }
//
//     if (proto_request.has_timeout_ms()) {
//         request.timeout_ms = proto_request.timeout_ms();
//     }
//
//     return request;
// }
//
// void VectorSearchServiceImpl::convert_to_proto_search_response(const QueryResponse& query_response,
//                                                              ::dann::SearchResponse* proto_response) const {
//     proto_response->set_success(query_response.success);
//     proto_response->set_error_message(query_response.error_message);
//     proto_response->set_query_time_ms(query_response.query_time_ms);
//
//     // Convert search results
//     for (const auto& result : query_response.results) {
//         auto* proto_result = proto_response->add_results();
//         *proto_result = convert_to_proto_search_result(result);
//     }
// }

} // namespace dann
