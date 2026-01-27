#pragma once

#include <grpc/grpc.h>
#include <grpcpp/grpcpp.h>
#include "dann/vector_index.h"
#include "vector_service.pb.h"
#include "vector_service.grpc.pb.h"
#include "dann/types.h"
#include <memory>

namespace dann {

class VectorSearchServiceImpl final : public dann::VectorSearchService::Service {
public:
    VectorSearchServiceImpl(std::shared_ptr<VectorIndex> vector_index);
    
    // gRPC service implementations
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
    std::shared_ptr<VectorIndex> vector_index_;

    // Helper methods
    // SearchResult convert_to_search_result(const ::dann::SearchResult& proto_result) const;
    // ::dann::SearchResult convert_to_proto_search_result(const SearchResult& result) const;
};

} // namespace dann
