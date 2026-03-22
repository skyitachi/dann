# MetaDataStorage

## 主要作用
- 负责管理每个索引的metadata, 和集群的metadata

## 主要接口
```c++
class MetaDataStorage
{
public:
    virtual ~MetaDataStorage() = default;
    virtual IndexMetaData& Get(const std::string& index_name) = 0;
    virtual void Put(const std::string& index_name, const IndexMetaData& metadata)=0;
}
```

### 索引的metadata数据结构
#### 示例
```json
{
  "index": "xxx",
  "node_ids": ["node_id1", "node_id2", "node_id3"],
  "type": "ivf",
  "dimension": 128,
  "routing_table": {
    "node_id1": {
      "role": "coordinate",
      "index_file": "main_index_file_path"
    },
    "node_id2": {
      "role": "data",
      "index_file": "data_file_path"
    },
    "node_id3": {
      "role": "data",
      "index_file": "data_file_path"
    }
  }
}
```

### main_index_file的存储结构
#### memory
```c++
// 优化：使用更紧凑的数据结构，提高缓存命中率
struct PostingAssignment {
    uint32_t centroid_start;
    uint32_t centroid_end;
    uint32_t node_id_index;       // 索引到 unique_node_ids，减少内存占用
};

class MainIndexStorage {
public:
    uint32_t magic_number;          // 文件魔数，用于验证文件格式
    uint32_t version;              // 文件格式版本
    uint32_t total_centroids;      // 质心总数
    uint32_t dimension;            // 向量维度
    uint32_t num_assignments;      // 分配记录数
    uint32_t num_unique_nodes;     // 唯一节点数量
    uint32_t checksum;             // 校验和（CRC32）
    std::vector<std::string> unique_node_ids;  // 去重后的节点ID列表（UUID或自定义字符串）
    std::vector<PostingAssignment> assigns_;   // 分配记录（使用索引引用 node_id）
    std::vector<float> centroids_;             // 质心向量（连续内存，便于向量化操作）

    // 辅助函数：获取指定索引的节点ID
    const std::string& GetNodeId(uint32_t index) const {
        return unique_node_ids[index];
    }
};
```

#### disk
```text
# Header (36 bytes)
magic_number(uint32) = 0x44414E4E  // "DANN"
version(uint32) = 1
total_centroids(uint32)
dimension(uint32)
num_assignments(uint32)
num_unique_nodes(uint32)          // 唯一节点数量
checksum(uint32)
reserved[4](uint32)               // 预留字段，用于未来扩展

# Unique Node IDs Table (variable size)
# 存储所有唯一的节点ID，每个ID可以是UUID或自定义字符串
node_id_length1(uint32), node_id1_bytes...
node_id_length2(uint32), node_id2_bytes...
...
node_id_lengthN(uint32), node_idN_bytes...

# Assignments Table (variable size)
# 每个条目使用索引引用 node_id，减少存储
for i = 1 to num_assignments:
    centroid_start(uint32)
    centroid_end(uint32)
    node_id_index(uint32)          // 索引到 Unique Node IDs Table

# Centroids Data (fixed size)
# 连续存储，便于内存映射和向量化操作
centroid_vectors_data = sizeof(float) * dimension * total_centroids

# Optional: Footer for quick validation
footer_checksum(uint32)
footer_magic(uint32) = 0x4E4E4144  // "NNAD"
```

#### 优化说明
1. **内存优化：**
   - 使用 `uint32_t` 替代 `int`，明确大小，便于序列化
   - `node_id` 保持为字符串（UUID 或自定义格式），保证全局唯一性
   - 分离 `unique_node_ids` 为独立向量，避免重复字符串存储
   - 在 assignments 中使用索引引用 node_id，大幅减少内存占用（尤其对于大规模集群）

2. **磁盘优化：**
   - 添加 `magic_number` 和 `version`，支持文件格式验证和版本兼容
   - 添加 `checksum` 校验数据完整性
   - 预留字段 `reserved` 支持未来扩展
   - 固定大小的 header (36字节)，便于快速定位
   - 分离 unique node IDs table 和 assignments table，提高压缩率
   - assignments table 使用索引引用 node_id，避免重复存储相同的 node_id
   - 连续存储质心向量，便于内存映射和SIMD优化

3. **性能优化：**
   - 支持内存映射（mmap），减少数据拷贝
   - 连续的向量存储，便于SIMD指令加速
   - 预留字段支持增量更新（如 append-only）
   - 压缩选项（如可选的 float16 存储）
   - 去重的 node_id 存储减少内存占用和磁盘I/O

4. **扩展性：**
   - version 字段支持向后兼容的文件格式演进
   - reserved 字段可用于添加元数据（如压缩类型、索引统计等）
   - 可选的 footer checksum 支持快速验证
   - 支持标准 UUID 格式（如 "550e8400-e29b-41d4-a716-446655440000"）
   - 支持自定义字符串格式作为 node_id（保证全局唯一）

5. **Node ID 格式建议：**
   - **UUID 标准：** 使用 RFC 4122 格式，如 `550e8400-e29b-41d4-a716-446655440000`（36字符）
   - **短UUID：** 使用压缩格式，如 `0x550e8400e29b41d4a716446655440000`（32字符）
   - **自定义ID：** 使用业务相关的字符串，如 `region1-shard001`（保证全局唯一）
   - **Base64编码：** 对于二进制ID，使用Base64编码存储

#### 使用建议
- 对于大规模索引，优先使用内存映射方式加载
- 对于高维向量（d > 256），考虑使用 float16 压缩减少磁盘占用
- 定期校验 checksum，防止静默数据损坏
- 使用标准 UUID 格式保证跨平台兼容性
- 对于大量相同节点，去重存储可节省 50-90% 磁盘空间

### data_file的存储结构
```text
# Header (32 bytes)
magic_number(uint32) = 0x44415441  // "DATA"
version(uint32) = 1
total_postings(uint32)
index_table_size(uint32)
data_section_offset(uint32)
checksum(uint32)
reserved[3](uint32)

# Index Table (variable size, for quick lookup)
# 每个条目12字节，便于二分查找
for i = 1 to total_postings:
    posting_id(uint32)
    offset(uint32)     // 在data section中的偏移量
    length(uint32)     // posting数据长度（字节）

# Data Section (variable size)
# 每个posting包含ID列表和向量数据
for i = 1 to total_postings:
    # Posting Header
    num_vectors(uint32)         // 向量数量
    vector_ids_size(uint32)     // ID列表大小（字节）
    vector_data_size(uint32)    // 向量数据大小（字节）

    # Vector IDs (variable size)
    vector_ids[0](uint64), vector_ids[1](uint64), ..., vector_ids[num_vectors-1](uint64)

    # Vector Data (fixed size per posting)
    vector_data[0..num_vectors-1] = sizeof(float) * dimension * num_vectors

# Optional: Footer
footer_checksum(uint32)
footer_magic(uint32) = 0x41444154  // "TADA"
```

#### 优化说明
1. **性能优化：**
   - 独立的 Index Table 支持快速随机访问（二分查找 O(log n)）
   - 明确的 section 偏移，支持增量读取和内存映射
   - 固定大小的 header，便于快速元数据访问
   - 使用 `uint64_t` 存储 vector IDs，支持更大的ID空间

2. **存储优化：**
   - 将向量ID和向量数据分离，便于压缩和缓存
   - 预留字段支持未来特性（如压缩算法、索引类型等）
   - 双重校验（header checksum + footer checksum）增强数据完整性

3. **扩展性：**
   - version 字段支持文件格式演进
   - 可选的 footer 支持追加写入（append-only）
   - 灵活的 posting 结构支持不同类型的向量数据

4. **使用建议：**
   - 对于频繁查询的 posting，可将其 data section 预加载到内存
   - 大规模 posting 可采用 LRU 缓存策略
   - 考虑对向量数据进行压缩（如 LZ4、ZSTD）
   - 使用 SSD 存储以提高随机I/O性能

## 设计总结

### 关键优化点
1. **内存效率：**
   - 使用 `uint32_t` 替代 `int`，明确内存布局
   - node_id 去重并使用索引引用，减少内存占用（尤其是大规模集群）
   - 结构体分离（SoA），提高缓存命中率

2. **磁盘性能：**
   - 固定大小 header，快速元数据访问
   - 独立的索引表，支持 O(log n) 查找
   - 连续向量存储，便于内存映射和SIMD优化

3. **数据完整性：**
   - 魔数验证文件格式
   - 版本号支持向后兼容
   - 双重校验和（CRC32）

4. **扩展性：**
   - 预留字段支持未来特性
   - 灵活的 section 布局
   - 支持 append-only 更新模式
   - node_id 支持多种格式（UUID、自定义字符串）

### 未来扩展方向
1. **压缩：** 支持 float16、int8 量化，减少存储空间
2. **加密：** 可选的字段级加密，保护敏感数据
3. **分片：** 支持多文件分片，处理超大规模索引
4. **缓存：** 内置 LRU/Bloom Filter 缓存策略
5. **监控：** 添加访问统计和性能指标