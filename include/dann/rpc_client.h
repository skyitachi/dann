#pragma once

#include <grpc++/grpc++.h>
#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include "vector_service.pb.h"
#include "vector_service.grpc.pb.h"

namespace dann {

class RPCClient {
public:
    RPCClient(const std::string& address, int port);
    ~RPCClient();

    bool connect();
    bool disconnect();
    bool is_connected() const;

    grpc::Status Search(const SearchRequest& request, SearchResponse* response);
    grpc::Status AddVectors(const AddVectorsRequest& request, AddVectorsResponse* response);
    grpc::Status RemoveVector(const RemoveVectorRequest& request, RemoveVectorResponse* response);
    grpc::Status UpdateVector(const UpdateVectorRequest& request, UpdateVectorResponse* response);
    grpc::Status GetVector(const GetVectorRequest& request, GetVectorResponse* response);
    grpc::Status GetStats(const StatsRequest& request, StatsResponse* response);
    grpc::Status HealthCheck(const HealthCheckRequest& request, HealthCheckResponse* response);

    void set_timeout_ms(int timeout_ms);

private:
    std::unique_ptr<grpc::ClientContext> create_context() const;

    std::string address_;
    int port_;
    std::atomic<bool> connected_;
    int timeout_ms_;

    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<VectorSearchService::Stub> stub_;
};

} // namespace dann
