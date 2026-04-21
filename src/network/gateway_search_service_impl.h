#pragma once

#include <grpc/grpc.h>
#include <grpcpp/grpcpp.h>
#include "vector_service.pb.h"
#include "vector_service.grpc.pb.h"
#include "dann/rpc_client.h"

#include <memory>
#include <vector>
#include <string>
#include <mutex>

namespace dann {

struct ShardInfo {
    int shard_id;
    std::string address;
    int port;
    std::unique_ptr<RPCClient> client;
};

class GatewaySearchServiceImpl final : public dann::VectorSearchService::Service {
public:
    explicit GatewaySearchServiceImpl(int shard_count, int dimension);

    void add_shard(int shard_id, const std::string& address, int port);
    bool connect_all_shards();

    grpc::Status Search(grpc::ServerContext* context,
                        const dann::SearchRequest* request,
                        dann::SearchResponse* response) override;

    grpc::Status AddVectors(grpc::ServerContext* context,
                           const dann::AddVectorsRequest* request,
                           dann::AddVectorsResponse* response) override;

    grpc::Status RemoveVector(grpc::ServerContext* context,
                             const dann::RemoveVectorRequest* request,
                             dann::RemoveVectorResponse* response) override;

    grpc::Status UpdateVector(grpc::ServerContext* context,
                             const dann::UpdateVectorRequest* request,
                             dann::UpdateVectorResponse* response) override;

    grpc::Status GetVector(grpc::ServerContext* context,
                          const dann::GetVectorRequest* request,
                          dann::GetVectorResponse* response) override;

    grpc::Status GetStats(grpc::ServerContext* context,
                         const dann::StatsRequest* request,
                         dann::StatsResponse* response) override;

    grpc::Status HealthCheck(grpc::ServerContext* context,
                            const dann::HealthCheckRequest* request,
                            dann::HealthCheckResponse* response) override;

private:
    int shard_id_for_document(int64_t id) const;

    int shard_count_;
    int dimension_;
    std::vector<ShardInfo> shards_;
    mutable std::mutex shards_mutex_;
};

} // namespace dann
