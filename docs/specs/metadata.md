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
```text
total_centroids_size(int32)
assignments: centroid_id->node_id
centroid_vectors_data =  sizeof(float) * dimension * total_centroids
```

### data_file的存储结构
```text
total_postings(int32)
posting_id1(int32),offset1(int32),len1(int32)
posting_id2,offset2,len2(int32)
...
posting_idn,offset_n,len_N(int32)
posting_id1
vector_ids
vector_data
...
posting_idn
vector_ids
vector_data
```