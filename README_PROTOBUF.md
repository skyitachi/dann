# Protocol Buffers for DANN

DANN使用Protocol Buffers (protobuf) 来定义gRPC服务的接口和数据结构。

## 文件结构

```
proto/
├── vector_service.proto      # 向量搜索服务
├── node_management.proto     # 节点管理服务
├── consistency.proto         # 一致性管理服务
└── build_proto.sh          # 构建脚本
```

## 服务定义

### 1. VectorSearchService

提供向量搜索和索引管理功能：

- `Search` - 向量相似性搜索
- `AddVectors` - 批量添加向量
- `RemoveVector` - 删除向量
- `UpdateVector` - 更新向量
- `GetVector` - 获取向量
- `GetStats` - 获取索引统计
- `HealthCheck` - 健康检查

### 2. NodeManagementService

管理集群节点和分片：

- `JoinCluster` - 加入集群
- `LeaveCluster` - 离开集群
- `Heartbeat` - 心跳检测
- `GetClusterNodes` - 获取集群节点信息
- `AssignShards` - 分片分配
- `GetShardInfo` - 获取分片信息
- `ReplicateData` - 数据复制
- `SyncData` - 数据同步

### 3. ConsistencyService

处理分布式一致性：

- `PropagateOperation` - 操作传播
- `ResolveConflict` - 冲突解决
- `GetVectorClock` - 获取向量时钟
- `UpdateVectorClock` - 更新向量时钟
- `AntiEntropySync` - 反熵同步

## 构建步骤

### 1. 安装依赖

```bash
# macOS
brew install protobuf grpc

# Ubuntu/Debian
apt-get install protobuf-compiler libprotobuf-dev libgrpc++-dev
```

### 2. 生成protobuf文件

```bash
cd proto
./build_proto.sh
```

或者手动生成：

```bash
# 创建输出目录
mkdir -p include/generated
mkdir -p src/generated

# 生成C++文件
protoc --cpp_out=src/generated --proto_path=proto proto/vector_service.proto
protoc --cpp_out=src/generated --proto_path=proto proto/node_management.proto
protoc --cpp_out=src/generated --proto_path=proto proto/consistency.proto

# 移动头文件
mv src/generated/*.h include/generated/
```

### 3. 编译项目

```bash
cd build
cmake ..
make -j$(nproc)
```

## 数据结构

### Vector
```protobuf
message Vector {
  int64_t id = 1;
  repeated float data = 2;
  map<string, string> metadata = 3;
}
```

### SearchResult
```protobuf
message SearchResult {
  int64_t id = 1;
  float distance = 2;
  repeated float vector = 3;
  map<string, string> metadata = 4;
}
```

### NodeInfo
```protobuf
message NodeInfo {
  string node_id = 1;
  string address = 2;
  int32 port = 3;
  bool is_active = 4;
  int64_t last_heartbeat = 5;
  repeated int32 shard_ids = 6;
  map<string, string> metadata = 7;
  string version = 8;
  int64_t join_time = 9;
}
```

### VectorClock
```protobuf
message VectorClock {
  repeated VectorClockEntry entries = 1;
}

message VectorClockEntry {
  string node_id = 1;
  int64_t timestamp = 2;
}
```

## 使用示例

### 客户端调用

```cpp
#include "generated/vector_service.grpc.pb.h"

// 创建客户端
std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel("localhost:8080", 
    grpc::InsecureChannelCredentials());
std::unique_ptr<VectorSearchService::Stub> stub = VectorSearchService::NewStub(channel);

// 搜索请求
SearchRequest request;
for (float val : query_vector) {
    request.add_query_vector(val);
}
request.set_k(10);

SearchResponse response;
grpc::ClientContext context;
grpc::Status status = stub->Search(&context, request, &response);

if (status.ok()) {
    for (const auto& result : response.results()) {
        std::cout << "ID: " << result.id() << ", Distance: " << result.distance() << std::endl;
    }
}
```

### 服务端实现

```cpp
#include "generated/vector_service.grpc.pb.h"

class VectorSearchServiceImpl : public VectorSearchService::Service {
public:
    grpc::Status Search(grpc::ServerContext* context,
                      const SearchRequest* request,
                      SearchResponse* response) override {
        // 实现搜索逻辑
        auto results = vector_index_->search(request->query_vector(), request->k());
        
        response->set_success(true);
        for (const auto& result : results) {
            SearchResult* proto_result = response->add_results();
            proto_result->set_id(result.id);
            proto_result->set_distance(result.distance);
            for (float val : result.vector) {
                proto_result->add_vector(val);
            }
        }
        
        return grpc::Status::OK;
    }
    
    // 实现其他方法...
};
```

## 版本兼容性

- Protocol Buffers v3
- gRPC v1.57.0+
- C++17+

## 注意事项

1. **字段编号**: protobuf字段编号一旦分配不应更改
2. **向后兼容**: 添加新字段时保持向后兼容
3. **数据大小**: 注意序列化后的数据大小，特别是大向量数据
4. **错误处理**: gRPC调用需要适当的错误处理
5. **流式传输**: 对于大数据量考虑使用流式RPC

## 扩展

添加新的服务或消息：

1. 在相应的`.proto`文件中定义
2. 重新生成protobuf文件
3. 实现服务端和客户端代码
4. 更新CMakeLists.txt如果需要
