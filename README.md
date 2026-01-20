# DANN - Distributed Approximate Nearest Neighbors

基于FAISS的分布式向量数据库，支持最终一致性、分布式查询和批量加载。

## 特性

- **分布式架构**: 支持多节点集群部署
- **最终一致性**: 使用向量时钟实现最终一致性
- **高性能查询**: 基于FAISS的高效向量检索
- **批量加载**: 支持大规模向量数据的批量导入
- **负载均衡**: 多种负载均衡策略
- **容错机制**: 节点故障检测和自动恢复
- **查询缓存**: 提升重复查询性能

## 架构组件

### 核心组件

- **VectorIndex**: FAISS索引封装，支持IVF、HNSW、Flat等索引类型
- **NodeManager**: 节点管理，负责集群发现、健康监控、分片管理
- **ConsistencyManager**: 一致性管理，实现向量时钟和冲突解决
- **QueryRouter**: 查询路由，支持分布式查询和结果合并
- **BulkLoader**: 批量加载，支持并行导入和进度监控

### 支持的索引类型

- **Flat**: 精确搜索，适合小数据集
- **IVF**: 倒排索引，平衡精度和速度
- **HNSW**: 图索引，高精度和高速度

## 编译和安装

### 依赖项

- CMake 3.16+
- C++17 编译器
- FAISS 1.7.4+
- gRPC 1.57.0+
- OpenSSL
- Redis (可选，用于分布式缓存)

### 编译

```bash
# 克隆代码
git clone <repository-url>
cd dann

# 创建构建目录
mkdir build && cd build

# 配置和编译
cmake ..
make -j$(nproc)

# 运行测试
make test
```

## 使用方法

### 启动单个节点

```bash
./dann_server --node-id node1 --port 8080 --dimension 128 --index-type IVF
```

### 启动集群

```bash
# 节点1 (种子节点)
./dann_server --node-id node1 --port 8080 --dimension 128

# 节点2
./dann_server --node-id node2 --port 8081 --seed-nodes node1:8080

# 节点3
./dann_server --node-id node3 --port 8082 --seed-nodes node1:8080
```

### 命令行参数

- `--node-id <id>`: 节点标识符
- `--address <addr>`: 监听地址 (默认: 0.0.0.0)
- `--port <port>`: 监听端口 (默认: 8080)
- `--dimension <dim>`: 向量维度 (默认: 128)
- `--index-type <type>`: 索引类型 (Flat/IVF/HNSW, 默认: IVF)
- `--seed-nodes <nodes>`: 种子节点列表，逗号分隔

## API 使用示例

### 基本操作

```cpp
#include "dann/vector_index.h"
#include "dann/query_router.h"
#include "dann/bulk_loader.h"

// 创建索引
auto index = std::make_shared<VectorIndex>(128, "IVF");

// 添加向量
std::vector<float> vectors = {/* 向量数据 */};
std::vector<int64_t> ids = {/* 向量ID */};
index->add_vectors(vectors, ids);

// 搜索
std::vector<float> query = {/* 查询向量 */};
auto results = index->search(query, 10);
```

### 批量加载

```cpp
auto bulk_loader = std::make_shared<BulkLoader>(index, consistency_manager);

BulkLoadRequest request(vectors, ids, 1000);
auto future = bulk_loader->load_vectors(request);

// 监控进度
auto progress = bulk_loader->get_progress(load_id);
std::cout << "Progress: " << progress.progress_percentage << "%" << std::endl;

// 等待完成
bool success = future.get();
```

### 分布式查询

```cpp
auto query_router = std::make_shared<QueryRouter>(node_manager);

QueryRequest request(query_vector, 10);
auto response = query_router->execute_query(request);

if (response.success) {
    for (const auto& result : response.results) {
        std::cout << "ID: " << result.id << ", Distance: " << result.distance << std::endl;
    }
}
```

## 性能优化

### 索引选择

- **小数据集 (< 10K)**: 使用 Flat 索引
- **中等数据集 (10K-1M)**: 使用 IVF 索引
- **大数据集 (> 1M)**: 使用 HNSW 索引

### 批量加载优化

```cpp
// 设置合适的批次大小
bulk_loader->set_batch_size(1000);

// 设置并发加载数
bulk_loader->set_max_concurrent_loads(4);

// 启用索引优化
bulk_loader->optimize_index_after_load();
```

### 查询优化

```cpp
// 启用查询缓存
query_router->enable_caching(true);

// 设置负载均衡策略
query_router->set_load_balance_strategy("least_loaded");
```

## 监控和指标

### 查询指标

```cpp
auto metrics = query_router->get_metrics();
std::cout << "Total queries: " << metrics.total_queries << std::endl;
std::cout << "Average response time: " << metrics.avg_response_time_ms << " ms" << std::endl;
```

### 加载指标

```cpp
auto metrics = bulk_loader->get_metrics();
std::cout << "Vectors loaded: " << metrics.total_vectors_loaded << std::endl;
std::cout << "Average load speed: " << metrics.avg_vectors_per_second << " vectors/sec" << std::endl;
```

## 配置文件

支持通过配置文件设置参数：

```json
{
  "node": {
    "id": "node1",
    "address": "0.0.0.0",
    "port": 8080
  },
  "index": {
    "dimension": 128,
    "type": "IVF",
    "parameters": {
      "nlist": 1000,
      "nprobe": 10
    }
  },
  "cluster": {
    "seed_nodes": ["node1:8080", "node2:8081"],
    "replication_factor": 3
  },
  "performance": {
    "batch_size": 1000,
    "max_concurrent_loads": 4,
    "cache_enabled": true
  }
}
```

## 故障排除

### 常见问题

1. **编译错误**: 确保安装了所有依赖项
2. **内存不足**: 调整批次大小和并发数
3. **查询慢**: 检查索引类型和参数设置
4. **节点连接失败**: 检查网络配置和防火墙设置

### 日志调试

```cpp
// 启用详细日志
spdlog::set_level(spdlog::level::debug);

// 查看集群状态
auto nodes = node_manager->get_cluster_nodes();
for (const auto& node : nodes) {
    std::cout << "Node: " << node.node_id << ", Active: " << node.is_active << std::endl;
}
```

## 贡献

欢迎提交Issue和Pull Request！

## 许可证

MIT License
