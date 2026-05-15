# Node 节点

## 作用
- 集群中实际承载流量与计算的最小运行单元,封装节点身份、角色、生命周期与对外 RPC
- 通过 RPC 与其它节点通信,组成 coordinate / data 两类角色协同的分布式 IVF 检索服务
- 与 [[metadata]] 配合从 `MetaDataStorage` 拉取索引路由表 (`routing_table`),与 [[datastore]] 配合加载 / 持久化索引文件

## 角色 (NodeRole)
| 角色 | 主要职责 | 数据归属 |
|---|---|---|
| `coordinate` | 1. `Search` 入口,接 client 请求<br>2. 加载 `main_index_file`(质心+路由),做粗粒度路由<br>3. 把 query 分发到目标 data 节点,合并 top-k<br>4. `BuildIndex` 入口:聚类、生成 main_index、给 data 分配 posting<br>5. 拉取 / 缓存集群与索引 metadata | 仅持有质心和路由信息,不持有原始向量 |
| `data` | 1. 加载 `data_file`(posting + 向量)<br>2. 在指定 posting 内做精排(brute-force 或 PQ)<br>3. 写入新向量到本地 posting,周期 flush 到 `DataStore`<br>4. 上报心跳 / 加载状态 | 持有真实向量数据(按 posting 分片) |

> 一个进程可以同时承担多个角色(单机模式或小集群),通过 `NodeConfig.roles` 位图控制;但同一节点对同一索引只承担一个角色,以 `IndexMetaData.routing_table` 为准。

## 关键数据结构

```c++
enum class NodeRole : uint8_t {
    kUnknown    = 0,
    kCoordinate = 1 << 0,
    kData       = 1 << 1,
};

struct NodeAddress {
    std::string node_id;   // 全局唯一,与 MetaDataStorage 中的 node_id 对齐(UUID / 自定义)
    std::string host;      // ip 或 dns 名
    uint16_t    port;      // gRPC 监听端口
};

struct NodeConfig {
    NodeAddress             self;
    uint32_t                roles;          // NodeRole 位掩码
    std::string             data_dir;       // 本地缓存目录
    uint32_t                io_threads = 8;
    uint32_t                worker_threads = 0;  // 0 = hardware_concurrency
    std::chrono::milliseconds heartbeat_interval{3000};
    std::chrono::milliseconds rpc_timeout{2000};
};
```

## 节点接口

```c++
class Node {
public:
    virtual ~Node() = default;

    // ==== 生命周期 ====
    virtual Status Start() = 0;            // 启动 RPC server、加载本地缓存
    virtual Status Stop()  = 0;            // 优雅停机:停 server、flush 数据、注销
    virtual NodeRole Roles() const = 0;
    virtual const NodeAddress& Self() const = 0;

    // ==== 元数据 ====
    // 拉取 / 刷新索引 metadata,coordinate 启动时调用以构建路由
    virtual Status LoadIndexMetadata(const std::string& index_name) = 0;
    virtual Status ReloadRoutingTable(const std::string& index_name) = 0;

    // ==== 客户端可见 RPC(由 coordinate 暴露) ====
    virtual Status Search(const SearchRequest&  req, SearchResponse*  resp) = 0;
    virtual Status BuildIndex(const BuildIndexRequest& req, BuildIndexResponse* resp) = 0;

    // ==== 集群内 RPC(节点之间) ====
    virtual Status ShardSearch(const ShardSearchRequest& req, ShardSearchResponse* resp) = 0;  // data 实现
    virtual Status LoadShard  (const LoadShardRequest&   req, LoadShardResponse*   resp) = 0;  // data 实现
    virtual Status DropShard  (const DropShardRequest&   req, DropShardResponse*   resp) = 0;  // data 实现
    virtual Status Heartbeat  (const HeartbeatRequest&   req, HeartbeatResponse*   resp) = 0;  // 双向

    // ==== 监控 ====
    virtual NodeStats GetStats() const = 0;
};
```

## RPC 通信

### 协议
- 统一使用 gRPC + protobuf;复用 [[vector_service]] 已有 `Search/AddVectors/...`,新增 `NodeService` 用于集群内通信
- 默认服务发现:`dns:///<svc>:<port>` + `round_robin`,可通过自定义 resolver 接入 `MetaDataStorage` 中的 `routing_table` 获取最新节点列表

### service NodeService(集群内)
```proto
service NodeService {
  // data 节点接收 coordinate 的分片查询
  rpc ShardSearch(ShardSearchRequest) returns (ShardSearchResponse);

  // 让 data 节点加载 / 卸载某索引下的 posting 集合
  rpc LoadShard(LoadShardRequest)     returns (LoadShardResponse);
  rpc DropShard(DropShardRequest)     returns (DropShardResponse);

  // 心跳与状态上报
  rpc Heartbeat(HeartbeatRequest)     returns (HeartbeatResponse);

  // 触发本地从 DataStore 重新加载索引文件
  rpc ReloadIndex(ReloadIndexRequest) returns (ReloadIndexResponse);
}

message ShardSearchRequest {
  string index_name = 1;
  bytes  query_vec  = 2;     // 原始 float 数组,长度=dimension*sizeof(float)
  repeated uint32 posting_ids = 3;  // coordinate 决定要扫的 posting
  uint32 top_k = 4;
  uint32 trace_id = 5;       // 链路追踪
}

message ShardSearchResponse {
  repeated uint64 ids       = 1;
  repeated float  distances = 2;
  uint32 visited_postings   = 3;   // 用于监控
}
```

### 客户端连接管理
- 每个 Node 内置 `NodeClientPool`,按 `node_id` 缓存 `grpc::Channel`,配置:
  - `keepalive_time = 30s`,`keepalive_timeout = 5s`
  - `max_inflight_per_channel`,超过时同一 node 复用多 channel(避免单 HTTP/2 队头阻塞)
  - 失败重试由调用方决定;`ShardSearch` 在 deadline 内允许 1 次重试,build/load 类不重试
- 路由表更新时发布事件,`NodeClientPool` 异步淘汰下线 channel

## 关键流程

### Search(coordinate 主导)
```
client ──► coordinate.Search
              │
              ├─ 校验 index_name,确保 metadata 已加载
              ├─ 在本地 main_index 上对 query 求 nprobe 个最近质心
              ├─ 查 routing_table:posting_id → node_id
              ├─ 按 node_id 聚合 posting_id 列表,并发发起 ShardSearch
              ├─ 等待结果(deadline = rpc_timeout 的 80%),归并 top-k
              └─ 返回给 client
```
- **优化点**
  1. nprobe 个 posting 可能落在同一 data 节点上 → 一次 RPC 携带多 `posting_ids`,减少连接放大
  2. coordinate 内部 query 解析、归并使用 `io_thread_pool`,避免阻塞 RPC handler
  3. 返回前可附带 `visited_postings` 等指标,coordinate 写到 metrics

### BuildIndex(coordinate 主导)
```
client ──► coordinate.BuildIndex
              ├─ 训练质心(本地 / 复用聚类工具)
              ├─ 写 main_index_file 到 DataStore
              ├─ 按一致性哈希或负载均衡决定 posting → node_id 映射
              ├─ 更新 MetaDataStorage 的 routing_table
              ├─ 并发对每个 data 节点调用 LoadShard(下发要负责的 posting 列表 + 文件路径)
              └─ 全部确认后,返回 build 完成
```

### data 节点加载
- 收到 `LoadShard` 后,从 `DataStore` 拉取 `data_file`,按 `posting_id` 解析 index table,完成后向 coordinate 回 ACK
- 支持懒加载:首次 `ShardSearch` 命中未加载 posting 时再读盘,read-through cache

### 心跳与故障
- data → coordinate 周期 `Heartbeat`,携带 loaded posting 列表 + 资源水位(mem/cpu/qps)
- coordinate 发现 data 不健康:
  1. 标记 routing_table 中该 node 状态 `unhealthy`(写入 MetaDataStorage)
  2. 短期内 fallback 到副本节点(若 posting 有副本)
  3. 长期触发再均衡,重新分配 posting

## 配置示例
```yaml
node:
  self:
    node_id: "550e8400-e29b-41d4-a716-446655440000"
    host: "10.0.1.7"
    port: 50051
  roles: ["coordinate", "data"]   # 单机部署时同时承担两种
  data_dir: "/var/dann"
  io_threads: 16
  heartbeat_interval_ms: 3000
  rpc_timeout_ms: 2000
metadata:
  backend: "filesystem"            # 见 [[metadata]]
  path: "/var/dann/meta"
datastore:
  backend: "filesystem"            # 见 [[datastore]]
  path: "/var/dann/data"
```

## 设计要点

1. **角色解耦**
   - 角色用位掩码,允许同进程多角色,便于本地调试与小规模集群部署
   - 但路由仍以 `IndexMetaData.routing_table` 为准,避免逻辑分叉

2. **状态最小化**
   - Node 自身不持有持久状态:metadata 在 `MetaDataStorage`,数据在 `DataStore`,本地仅做缓存
   - 重启后通过 `LoadIndexMetadata` + `LoadShard` 恢复

3. **路由与发现分离**
   - 节点间地址发现走 DNS / xDS / 自定义 resolver(见上文)
   - 索引级别的 posting → node 映射只在 `routing_table`,与服务发现解耦

4. **可观测性**
   - 每个 RPC 携带 `trace_id`,coordinate 在归并时落 metrics(p50/p99 latency、命中 posting 数、重试次数)
   - `NodeStats` 暴露:loaded postings、qps、mem、最近一次 metadata 同步时间

5. **演进方向**
   - posting 副本与读写分离(主副本写,任意副本读)
   - coordinate 选主,支持多 coordinate 并行接流量(基于 lease)
   - 异步 build / 增量 build,允许 build 期间不下线
   - 引入 `xds` resolver,接入 service mesh
