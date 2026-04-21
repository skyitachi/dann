#include "dann/rpc_client.h"
#include <grpc++/create_channel.h>
#include <chrono>

namespace dann {

RPCClient::RPCClient(const std::string& address, int port)
    : address_(address), port_(port), connected_(false), timeout_ms_(5000) {}

RPCClient::~RPCClient() {
    disconnect();
}

bool RPCClient::connect() {
    if (connected_.load()) {
        return true;
    }

    try {
        std::string server_address = address_ + ":" + std::to_string(port_);

        grpc::ChannelArguments args;
        args.SetMaxReceiveMessageSize(100 * 1024 * 1024);
        args.SetMaxSendMessageSize(100 * 1024 * 1024);

        channel_ = grpc::CreateCustomChannel(
            server_address, grpc::InsecureChannelCredentials(), args);

        if (!channel_) {
            return false;
        }

        stub_ = VectorSearchService::NewStub(channel_);
        if (!stub_) {
            return false;
        }

        connected_ = true;
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

bool RPCClient::disconnect() {
    if (!connected_.load()) {
        return true;
    }
    connected_ = false;
    stub_.reset();
    channel_.reset();
    return true;
}

bool RPCClient::is_connected() const {
    return connected_.load();
}

void RPCClient::set_timeout_ms(int timeout_ms) {
    timeout_ms_ = std::max(100, timeout_ms);
}

std::unique_ptr<grpc::ClientContext> RPCClient::create_context() const {
    auto ctx = std::make_unique<grpc::ClientContext>();
    ctx->set_deadline(std::chrono::system_clock::now() +
                      std::chrono::milliseconds(timeout_ms_));
    return ctx;
}

grpc::Status RPCClient::Search(const SearchRequest& request, SearchResponse* response) {
    if (!stub_) return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Not connected");
    auto ctx = create_context();
    return stub_->Search(ctx.get(), request, response);
}

grpc::Status RPCClient::AddVectors(const AddVectorsRequest& request, AddVectorsResponse* response) {
    if (!stub_) return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Not connected");
    auto ctx = create_context();
    return stub_->AddVectors(ctx.get(), request, response);
}

grpc::Status RPCClient::RemoveVector(const RemoveVectorRequest& request, RemoveVectorResponse* response) {
    if (!stub_) return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Not connected");
    auto ctx = create_context();
    return stub_->RemoveVector(ctx.get(), request, response);
}

grpc::Status RPCClient::UpdateVector(const UpdateVectorRequest& request, UpdateVectorResponse* response) {
    if (!stub_) return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Not connected");
    auto ctx = create_context();
    return stub_->UpdateVector(ctx.get(), request, response);
}

grpc::Status RPCClient::GetVector(const GetVectorRequest& request, GetVectorResponse* response) {
    if (!stub_) return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Not connected");
    auto ctx = create_context();
    return stub_->GetVector(ctx.get(), request, response);
}

grpc::Status RPCClient::GetStats(const StatsRequest& request, StatsResponse* response) {
    if (!stub_) return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Not connected");
    auto ctx = create_context();
    return stub_->GetStats(ctx.get(), request, response);
}

grpc::Status RPCClient::HealthCheck(const HealthCheckRequest& request, HealthCheckResponse* response) {
    if (!stub_) return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Not connected");
    auto ctx = create_context();
    return stub_->HealthCheck(ctx.get(), request, response);
}

} // namespace dann
