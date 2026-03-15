# DistributedIndexIVF 存储设计（仅 Flat 原始向量）

## 文档范围

本文描述如何在 `dann::DistributedIndexIVF` 中实现“接近 LanceDB IVF_FLAT 的存储结构”，约束如下：

- 暂不考虑 codec / 量化（PQ/SQ/RQ 均不在本阶段范围内）
- 向量统一按原始 `float32` 存储
- 仅给出代码架构与文件格式设计
- 本文不涉及实际代码改造

---

## 1. LanceDB IVF 的相关存储思想（本需求子集）

在 IVF_FLAT 场景下，LanceDB 的核心思想是把索引拆成两类文件：

- **索引结构文件**（`index.idx`）
  - 存 IVF centroid（聚类中心）
  - 存 partition 元信息（offsets、lengths）
  - 存索引级配置（distance type、index type 等）
- **向量载荷文件**（`auxiliary.idx`）
  - 存 row id
  - 存原始向量（或其他模式下的编码结果）
  - 按 partition 顺序落盘

这种分离让读取更高效：

1. 先读轻量元数据（centroid + partition 边界）
2. 查询时只读取 `nprobe` 需要的 partition 数据

---

## 2. dann 当前实现与目标差距

当前 `DistributedIndexIVF` + `IndexIVFShard` 主要是内存结构：

- `InvertedList { vector_ids, vectors }`
- 每个 shard 维护 `postings_` map

优点是简单直接；缺点是缺少：

- 索引结构与向量载荷的持久化分离
- 显式 partition 边界（offset/length）
- 按需加载 partition 的机制
- 索引元信息（manifest）用于版本与兼容校验

---

## 3. 目标架构（仅 Flat）

### 3.1 设计目标

- 保持现有 IVF 核心算法流程（train / assign / nprobe）
- 引入 Lance 风格双文件布局
- 保持分布式 shard 归属规则（`centroid % shard_count`）
- 让 `load_index` 可用且可扩展

### 3.2 目录与文件布局（每个索引实例）

建议目录：

`<index_path>/<index_name>/`

建议文件：

- `manifest.json`（可读元信息、版本、兼容信息）
- `index.idx`（二进制索引结构）
- `auxiliary.idx`（二进制向量载荷：row id + 原始向量）

### 3.3 职责划分

- 全局协调者 `DistributedIndexIVF` 负责：
  - centroid 与全局 partition 布局
  - `partition_id -> shard_id` 映射
  - 索引元信息读写
- 本地执行者 `IndexIVFShard` 负责：
  - 本 shard 的 partition 运行时缓存
  - 本 shard 的局部搜索（基于原始向量 brute-force）

---

## 4. 推荐数据结构

说明：名称仅为建议，可按现有命名风格微调。

### 4.1 索引元信息（manifest）

```cpp
enum class DistanceType : uint8_t {
    L2 = 0,
    COSINE = 1,
    DOT = 2
};

struct IvfIndexManifest {
    uint32_t format_version;      // 例如 1
    std::string index_name;
    int32_t dimension;
    int32_t nlist;
    int32_t nprobe_default;
    DistanceType distance_type;   // 当前 dann 默认可先用 L2
    int32_t shard_count;
    int64_t ntotal;
    bool trained;
};
```

### 4.2 Partition 描述符（结构侧）

```cpp
struct PartitionDescriptor {
    int32_t partition_id;         // 对应 centroid id
    int32_t shard_id;             // 归属 shard
    uint64_t aux_row_offset;      // 在 auxiliary.idx 的行偏移
    uint32_t length;              // partition 内向量数量
};
```

### 4.3 内存中的 Flat posting 容器

```cpp
struct FlatPostingList {
    std::vector<int64_t> row_ids; // 大小 N
    std::vector<float> vectors;   // 大小 N * dimension
};
```

### 4.4 运行时布局（由 index.idx 反序列化）

```cpp
struct IvfRuntimeLayout {
    std::vector<float> centroids;                 // nlist * dimension
    std::vector<PartitionDescriptor> partitions;  // nlist 项
    std::vector<int32_t> centroid_to_shard;       // nlist 项
};
```

---

## 5. 二进制文件格式设计（V1）

目标：先做最小可用、可维护的格式。

### 5.1 `index.idx`（索引结构）

Header：

- magic: `"DANNIVF\0"`（8 bytes）
- format_version: `uint32`
- dimension: `uint32`
- nlist: `uint32`
- shard_count: `uint32`
- ntotal: `uint64`
- distance_type: `uint8`
- reserved padding

Body：

1. centroids block（`float32[nlist * dimension]`）
2. partition 描述符 block（`PartitionDescriptor[nlist]`）

Footer：

- checksum（V1 可选，V2 建议强制）

### 5.2 `auxiliary.idx`（向量载荷）

Header：

- magic: `"DANNAUX\0"`（8 bytes）
- format_version: `uint32`
- dimension: `uint32`
- total_rows: `uint64`

Body（按 `partition_id` 升序写）：

对每个 partition 依次写：

- row_ids block（`int64[length]`）
- vectors block（`float32[length * dimension]`）

说明：`auxiliary.idx` 无需重复写每个 partition 头，因为边界信息已在 `index.idx` 的 `offset + length` 中给出。

---

## 6. dann 类级别设计

### 6.1 `DistributedIndexIVF`（设计建议）

新增职责：

1. 复用现有 build 流程构造 posting
2. 将结构层与载荷层分别落盘
3. 通过 `manifest + index.idx` 实现 `load_index(index_path)`
4. 查询时按 partition/shard 路由

建议新增私有方法：

```cpp
bool save_manifest(const std::string& dir, const IvfIndexManifest& m);
bool load_manifest(const std::string& dir, IvfIndexManifest* m);

bool save_index_structure(const std::string& dir, const IvfRuntimeLayout& layout);
bool load_index_structure(const std::string& dir, IvfRuntimeLayout* layout);

bool save_auxiliary_flat(const std::string& dir,
                         const std::vector<FlatPostingList>& postings_by_partition);
```

### 6.2 `IndexIVFShard`（设计建议）

当前 shard 直接持有完整 posting。V1 可保持该行为，同时增加“文件挂载 + 按需加载”路径。

建议新增方法：

```cpp
bool attach_auxiliary_file(const std::string& aux_file_path, int dimension);

bool load_partition_from_aux(const PartitionDescriptor& desc);

std::vector<InternalSearchResult> search_loaded_partitions(
    const std::vector<int64_t>& centroid_ids,
    const std::vector<float>& query,
    int k);
```

缓存策略（V1 简版）：

- 每个 shard 维护一个 `partition_id -> FlatPostingList` 的 LRU
- 可按“最大向量数”或“最大内存字节数”设上限

---

## 7. Build 与写盘流程（Flat）

1. 训练 centroid（`clustering_`）
2. 将每个向量分配到最近 centroid
3. 按 partition 构建 `postings_by_partition`
4. 计算 partition 元信息：
   - `length`
   - `aux_row_offset`（前缀和）
   - `shard_id = partition_id % shard_count`
5. 按 partition 顺序写 `auxiliary.idx`
6. 写 `index.idx`（centroids + descriptors）
7. 写 `manifest.json`
8. 设置 `is_trained_ = true`

---

## 8. Load 与查询流程

### 8.1 Load

1. 读取 `manifest.json`
2. 读取 `index.idx`
3. 校验：
   - dimension 是否匹配
   - `nlist` / `shard_count` / version 是否兼容
4. 初始化 `global_centroids_` 与 partition/shard 映射
5. 为 shard 挂载 `auxiliary.idx`

### 8.2 Search

1. 在 `global_centroids_` 上选 top-`nprobe` centroid
2. 按 `shard_id` 聚合 centroid 列表
3. 各 shard 执行：
   - 保证目标 partition 已加载（缓存命中或从 `auxiliary.idx` 读取）
   - 对原始向量做局部 brute-force 扫描
4. 汇总并合并全局 top-k

---

## 9. API 兼容策略

对外 API 保持不变：

- `add_vectors(...)`
- `build_index(...)`
- `search(...)`
- `load_index(...)`

建议演进方式：

- `build_index`：构建后可选择立即持久化
- `load_index`：补全为真正可加载持久化索引
- `search`：签名不变，内部可混合“内存 + 文件按需加载”实现

这样调用方无需改动。

---

## 10. 分阶段落地计划

阶段 1（低风险重构）：

- 引入 manifest / descriptor 等结构体
- 引入读写工具函数
- 搜索路径仍保持全内存

阶段 2（完成持久化）：

- 实现 `index.idx` 与 `auxiliary.idx` 写入/读取
- 打通 `load_index`

阶段 3（按需加载）：

- shard 增加 partition cache
- 查询仅加载命中的 partition

阶段 4（健壮性增强）：

- checksum
- 损坏检测与容错
- 版本兼容与降级策略

---

## 11. 测试建议（设计级）

单测：

- `index.idx` 序列化/反序列化正确性
- partition `offset/length` 计算正确性
- manifest 兼容校验逻辑

集成测试：

- Build -> Save -> Load -> Search 结果一致性
- 不同 shard 数量下结果稳定性
- 空索引 / 单 partition / 数据倾斜场景

正确性目标：

- 在 Flat 模式下，与当前内存实现保持同一距离度量与 top-k 语义的一致结果。

---

## 12. 该设计与当前 dann 的适配性

- 复用现有 IVF 训练与分桶流程
- 保持 `IndexIVFShard` 抽象不破坏
- 在不引入 codec 复杂度的前提下完成 Lance 风格的“结构/载荷分离”
- 为未来引入 PQ/SQ 预留扩展点（主要改载荷格式与 shard 扫描逻辑）

