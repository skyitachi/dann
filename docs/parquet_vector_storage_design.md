# Parquet 向量存储设计方案

## 1. 概述

使用 Apache Parquet + Arrow 存储和读取向量数据，提供与 `std::vector<float>` 兼容的访问接口，实现零拷贝向量计算。

## 2. Parquet 文件结构

```
┌─────────────────┬─────────────────────────────┐
│   id (INT64)    │   vector (FLOAT_ARRAY)      │
├─────────────────┼─────────────────────────────┤
│        0        │  [0.1, 0.2, 0.3, ...]       │
│        1        │  [0.4, 0.5, 0.6, ...]       │
│       ...       │            ...              │
└─────────────────┴─────────────────────────────┘
```

**Arrow Schema:**
```cpp
schema = arrow::schema({
    arrow::field("id", arrow::int64()),
    arrow::field("vector", arrow::list(arrow::float32()))
});
```

## 3. 核心类设计

### 3.1 ParquetVectorStore - 存储管理类

```cpp
class ParquetVectorStore {
public:
    // 加载 Parquet 文件（内存映射或加载到内存）
    bool load(const std::string& path);
    
    // 通过 ID 查找向量，返回 Arrow 数组视图
    arrow::FloatArray* find_vector(int64_t id);
    
    // 批量获取用于计算（零拷贝）
    const float* get_vector_data(int64_t id, size_t* dimension);
    
    // 遍历所有向量（类似 std::vector 迭代器）
    class Iterator {
        const float* data() const;   // 连续内存指针
        size_t dimension() const;    // 向量维度
        int64_t id() const;            // 当前 ID
    };
    
    // 随机访问（支持 [] 语义）
    VectorView operator[](int64_t id);
    
    // 批量查询优化
    std::vector<VectorView> batch_get(const std::vector<int64_t>& ids);
    
private:
    std::shared_ptr<arrow::Table> table_;
    std::shared_ptr<arrow::Int64Array> id_column_;
    std::shared_ptr<arrow::ListArray> vector_column_;
    std::unordered_map<int64_t, int64_t> id_to_index_;  // ID 到行索引的映射
};
```

### 3.2 VectorView - 向量视图类

提供与 `std::vector<float>` 完全兼容的接口：

```cpp
class VectorView {
public:
    // 构造：指向 Arrow 数组的原始数据（零拷贝）
    VectorView(const float* data, size_t size, int64_t id);
    
    // ==================== std::vector<float> 兼容接口 ====================
    
    // 容量
    size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    
    // 元素访问
    const float& operator[](size_t i) const { return data_[i]; }
    const float& at(size_t i) const { 
        if (i >= size_) throw std::out_of_range("Index out of range");
        return data_[i]; 
    }
    const float& front() const { return data_[0]; }
    const float& back() const { return data_[size_ - 1]; }
    
    // 迭代器（支持范围 for 循环）
    const float* begin() const { return data_; }
    const float* end() const { return data_ + size_; }
    const float* cbegin() const { return data_; }
    const float* cend() const { return data_ + size_; }
    
    // 原始数据指针（用于 BLAS/FAISS 计算）
    const float* data() const { return data_; }
    
    // ==================== 向量计算接口 ====================
    
    // 内积：直接操作连续内存，SIMD 友好
    float dot_product(const VectorView& other) const {
        assert(size_ == other.size_);
        float sum = 0.0f;
        for (size_t i = 0; i < size_; ++i) {
            sum += data_[i] * other.data_[i];
        }
        return sum;
    }
    
    // L2 距离
    float l2_distance(const VectorView& other) const {
        assert(size_ == other.size_);
        float sum = 0.0f;
        for (size_t i = 0; i < size_; ++i) {
            float diff = data_[i] - other.data_[i];
            sum += diff * diff;
        }
        return std::sqrt(sum);
    }
    
    // L2 距离平方（用于比较，避免开方）
    float l2_distance_square(const VectorView& other) const {
        assert(size_ == other.size_);
        float sum = 0.0f;
        for (size_t i = 0; i < size_; ++i) {
            float diff = data_[i] - other.data_[i];
            sum += diff * diff;
        }
        return sum;
    }
    
    // 元数据
    int64_t id() const { return id_; }
    
private:
    const float* data_;   // 指向 Arrow/Parquet 内存（零拷贝）
    size_t size_;         // 向量维度
    int64_t id_;          // 向量 ID
};
```

## 4. 核心实现

### 4.1 加载 Parquet 文件

```cpp
#include <arrow/parquet/reader.h>
#include <arrow/array.h>
#include <arrow/io/file.h>

bool ParquetVectorStore::load(const std::string& path) {
    // 方式1：普通文件读取
    auto result = arrow::io::ReadableFile::Open(path);
    if (!result.ok()) return false;
    auto file = *result;
    
    // 方式2：内存映射（大文件推荐）
    // auto mmap_result = arrow::io::MemoryMappedFile::Open(path, arrow::io::FileMode::READ);
    // if (!mmap_result.ok()) return false;
    // auto file = *mmap_result;
    
    // 创建 Parquet Reader
    auto reader_result = parquet::arrow::OpenFile(file, arrow::default_memory_pool());
    if (!reader_result.ok()) return false;
    reader_ = *reader_result;
    
    // 读取所有数据到 Arrow Table
    auto table_result = reader_->ReadTable();
    if (!table_result.ok()) return false;
    table_ = *table_result;
    
    // 获取列引用（零拷贝）
    id_column_ = std::static_pointer_cast<arrow::Int64Array>(
        table_->column(0)->chunk(0));
    vector_column_ = std::static_pointer_cast<arrow::ListArray>(
        table_->column(1)->chunk(0));
    
    // 构建 ID 到索引的映射（用于快速查找）
    for (int64_t i = 0; i < id_column_->length(); ++i) {
        id_to_index_[id_column_->Value(i)] = i;
    }
    
    return true;
}
```

### 4.2 向量查找（零拷贝）

```cpp
VectorView ParquetVectorStore::operator[](int64_t id) {
    auto it = id_to_index_.find(id);
    if (it == id_to_index_.end()) {
        return VectorView(nullptr, 0, -1);  // 无效向量
    }
    
    int64_t idx = it->second;
    
    // 从 ListArray 获取值数组（零拷贝）
    // vector_column_ 是 ListArray，每个元素是一个列表
    auto value_array = std::static_pointer_cast<arrow::FloatArray>(
        vector_column_->value_slice(idx)
    );
    
    return VectorView(
        value_array->raw_values(),    // 原始 float 指针
        value_array->length(),          // 向量维度
        id                              // 向量 ID
    );
}

// 批量获取（减少查找开销）
std::vector<VectorView> ParquetVectorStore::batch_get(const std::vector<int64_t>& ids) {
    std::vector<VectorView> results;
    results.reserve(ids.size());
    
    for (int64_t id : ids) {
        results.push_back((*this)[id]);
    }
    return results;
}
```

### 4.3 遍历所有向量

```cpp
class ParquetVectorStore::Iterator {
public:
    Iterator(const ParquetVectorStore* store, int64_t index) 
        : store_(store), index_(index) {
        update_view();
    }
    
    // 迭代器操作
    Iterator& operator++() {
        ++index_;
        update_view();
        return *this;
    }
    
    bool operator!=(const Iterator& other) const {
        return index_ != other.index_;
    }
    
    const VectorView& operator*() const { return current_view_; }
    const VectorView* operator->() const { return &current_view_; }
    
    // 访问当前数据
    int64_t id() const { return current_view_.id(); }
    const float* data() const { return current_view_.data(); }
    size_t dimension() const { return current_view_.size(); }
    
private:
    void update_view() {
        if (index_ < store_->id_column_->length()) {
            int64_t id = store_->id_column_->Value(index_);
            auto value_array = std::static_pointer_cast<arrow::FloatArray>(
                store_->vector_column_->value_slice(index_));
            current_view_ = VectorView(
                value_array->raw_values(),
                value_array->length(),
                id
            );
        }
    }
    
    const ParquetVectorStore* store_;
    int64_t index_;
    VectorView current_view_{nullptr, 0, -1};
};

// 范围遍历支持
ParquetVectorStore::Iterator ParquetVectorStore::begin() const {
    return Iterator(this, 0);
}

ParquetVectorStore::Iterator ParquetVectorStore::end() const {
    return Iterator(this, id_column_->length());
}
```

## 5. 写入 Parquet 文件

```cpp
#include <arrow/parquet/writer.h>
#include <arrow/builder.h>

bool write_vectors_to_parquet(const std::string& path,
                              const std::vector<int64_t>& ids,
                              const std::vector<std::vector<float>>& vectors) {
    assert(ids.size() == vectors.size());
    
    // 构建 Arrow Builders
    arrow::Int64Builder id_builder(arrow::default_memory_pool());
    arrow::ListBuilder list_builder(
        arrow::default_memory_pool(),
        std::make_shared<arrow::FloatBuilder>(arrow::default_memory_pool())
    );
    auto& float_builder = static_cast<arrow::FloatBuilder&>(*list_builder.value_builder());
    
    // 预留空间
    id_builder.Reserve(ids.size());
    list_builder.Reserve(ids.size());
    
    // 计算总元素数用于预留 float builder 空间
    size_t total_floats = 0;
    for (const auto& vec : vectors) {
        total_floats += vec.size();
    }
    float_builder.Reserve(total_floats);
    
    // 追加数据
    for (size_t i = 0; i < ids.size(); ++i) {
        id_builder.Append(ids[i]);
        list_builder.Append();
        float_builder.AppendValues(vectors[i].data(), vectors[i].size());
    }
    
    // 构建 Arrays
    auto id_array = id_builder.Finish().ValueOrDie();
    auto vector_array = list_builder.Finish().ValueOrDie();
    
    // 构建 Table
    auto schema = arrow::schema({
        arrow::field("id", arrow::int64()),
        arrow::field("vector", arrow::list(arrow::float32()))
    });
    
    auto table = arrow::Table::Make(schema, {id_array, vector_array}, ids.size());
    
    // 配置 Parquet 写入选项（启用压缩）
    auto write_options = parquet::arrow::WriteOptions();
    write_options.compression = parquet::Compression::SNAPPY;
    
    // 写入文件
    auto result = parquet::arrow::WriteTable(*table, 
                                              arrow::default_memory_pool(), 
                                              path, 
                                              write_options);
    return result.ok();
}
```

## 6. 与 FAISS 集成

```cpp
// 将 VectorView 数据传递给 FAISS 进行计算
void search_with_faiss(ParquetVectorStore& store,
                       const std::vector<int64_t>& candidate_ids,
                       const float* query_vector,
                       int k,
                       std::vector<int64_t>& result_ids,
                       std::vector<float>& result_distances) {
    // 获取候选向量
    auto views = store.batch_get(candidate_ids);
    
    // 构建连续内存缓冲区用于 FAISS
    size_t dim = views[0].size();
    std::vector<float> candidate_data;
    candidate_data.reserve(views.size() * dim);
    
    for (const auto& view : views) {
        candidate_data.insert(candidate_data.end(), 
                              view.begin(), view.end());
    }
    
    // 使用 FAISS 计算距离
    // faiss::knn_L2sqr(query_vector, candidate_data.data(), ...);
    
    // 或者直接在 VectorView 上计算（小批量时更快）
    std::vector<std::pair<float, int64_t>> distances;
    for (size_t i = 0; i < views.size(); ++i) {
        float dist = views[i].l2_distance(VectorView(query_vector, dim, -1));
        distances.emplace_back(dist, candidate_ids[i]);
    }
    
    // 排序取 Top-K
    std::partial_sort(distances.begin(), 
                      distances.begin() + k,
                      distances.end());
    
    for (int i = 0; i < k; ++i) {
        result_distances.push_back(distances[i].first);
        result_ids.push_back(distances[i].second);
    }
}
```

## 7. 性能优化策略

| 优化点 | 实现方式 | 适用场景 |
|-------|---------|---------|
| **零拷贝访问** | `arrow::FloatArray::raw_values()` 直接返回指针 | 所有计算场景 |
| **内存映射** | `arrow::io::MemoryMappedFile` | 大文件，内存有限 |
| **批量读取** | 按 Row Group 读取 | 顺序遍历 |
| **ID 索引** | `unordered_map<int64_t, int64_t>` | 随机访问 |
| **压缩存储** | Parquet Snappy/ZSTD 压缩 | 存储空间敏感 |
| **SIMD 计算** | `VectorView::data()` 返回连续内存，支持 AVX-512 | 大批量计算 |
| **列式读取** | 只读取 vector 列，跳过 id 列 | 纯计算场景 |

## 8. 使用示例

```cpp
int main() {
    // 1. 写入向量
    std::vector<int64_t> ids = {0, 1, 2, 3, 4};
    std::vector<std::vector<float>> vectors = {
        {0.1f, 0.2f, 0.3f, 0.4f},
        {0.5f, 0.6f, 0.7f, 0.8f},
        {0.9f, 1.0f, 1.1f, 1.2f},
        {1.3f, 1.4f, 1.5f, 1.6f},
        {1.7f, 1.8f, 1.9f, 2.0f}
    };
    write_vectors_to_parquet("vectors.parquet", ids, vectors);
    
    // 2. 加载存储
    ParquetVectorStore store;
    if (!store.load("vectors.parquet")) {
        std::cerr << "Failed to load parquet file" << std::endl;
        return 1;
    }
    
    // 3. 随机访问（类似 std::vector）
    VectorView vec = store[2];  // 获取 ID=2 的向量
    std::cout << "Vector 2: ";
    for (size_t i = 0; i < vec.size(); ++i) {
        std::cout << vec[i] << " ";
    }
    std::cout << std::endl;
    
    // 4. 范围遍历（类似 std::vector）
    for (const auto& view : store) {
        std::cout << "ID " << view.id() << ": ";
        for (float val : view) {
            std::cout << val << " ";
        }
        std::cout << std::endl;
    }
    
    // 5. 向量计算
    VectorView v1 = store[0];
    VectorView v2 = store[1];
    float dist = v1.l2_distance(v2);
    float dot = v1.dot_product(v2);
    
    std::cout << "L2 distance: " << dist << std::endl;
    std::cout << "Dot product: " << dot << std::endl;
    
    // 6. 批量查询
    std::vector<int64_t> query_ids = {0, 2, 4};
    auto batch = store.batch_get(query_ids);
    for (const auto& view : batch) {
        // 处理每个向量...
    }
    
    return 0;
}
```

## 9. 与 std::vector<float> 对比

| 特性 | std::vector<float> | ParquetVectorStore |
|-----|-------------------|-------------------|
| 内存布局 | 连续堆内存 | 内存映射/列式存储 |
| 随机访问 | O(1) | O(1)（有索引） |
| 序列化 | 自定义实现 | 标准 Parquet 格式 |
| 跨语言 | 需自定义 | Python/Rust/Java 原生支持 |
| 压缩 | 无 | Snappy/ZSTD |
| 启动加载 | 全量加载 | 延迟加载/内存映射 |
| 接口兼容性 | 原生 | 通过 VectorView 完全兼容 |

## 10. 头文件设计

```cpp
// dann/parquet_vector_store.h
#pragma once

#include <arrow/table.h>
#include <arrow/array.h>
#include <unordered_map>
#include <string>
#include <vector>

namespace dann {

class VectorView {
public:
    VectorView() = default;
    VectorView(const float* data, size_t size, int64_t id);
    
    // std::vector<float> 兼容接口
    size_t size() const;
    bool empty() const;
    const float& operator[](size_t i) const;
    const float& at(size_t i) const;
    const float& front() const;
    const float& back() const;
    const float* begin() const;
    const float* end() const;
    const float* data() const;
    
    // 向量计算
    float dot_product(const VectorView& other) const;
    float l2_distance(const VectorView& other) const;
    float l2_distance_square(const VectorView& other) const;
    
    int64_t id() const;
    
private:
    const float* data_ = nullptr;
    size_t size_ = 0;
    int64_t id_ = -1;
};

class ParquetVectorStore {
public:
    ParquetVectorStore() = default;
    ~ParquetVectorStore() = default;
    
    // 禁止拷贝，允许移动
    ParquetVectorStore(const ParquetVectorStore&) = delete;
    ParquetVectorStore& operator=(const ParquetVectorStore&) = delete;
    ParquetVectorStore(ParquetVectorStore&&) = default;
    ParquetVectorStore& operator=(ParquetVectorStore&&) = default;
    
    // 加载
    bool load(const std::string& path);
    
    // 访问
    VectorView operator[](int64_t id);
    VectorView at(int64_t id);
    
    // 批量访问
    std::vector<VectorView> batch_get(const std::vector<int64_t>& ids);
    
    // 遍历
    class Iterator;
    Iterator begin() const;
    Iterator end() const;
    
    // 元数据
    size_t size() const;
    bool empty() const;
    size_t dimension() const;
    
private:
    std::shared_ptr<arrow::Table> table_;
    std::shared_ptr<arrow::Int64Array> id_column_;
    std::shared_ptr<arrow::ListArray> vector_column_;
    std::unordered_map<int64_t, int64_t> id_to_index_;
    size_t dimension_ = 0;
};

// 写入工具函数
bool write_vectors_to_parquet(const std::string& path,
                              const std::vector<int64_t>& ids,
                              const std::vector<std::vector<float>>& vectors);

} // namespace dann
```
