//
// Arrow and Parquet Test
// Tests for Arrow/Parquet vector storage functionality
//

#include <gtest/gtest.h>
#include <arrow/api.h>
#include <arrow/io/api.h>
#include <arrow/ipc/api.h>
#include <parquet/arrow/reader.h>
#include <parquet/arrow/writer.h>
#include <arrow/array/builder_primitive.h>
#include <arrow/array/builder_nested.h>

#include <vector>
#include <string>
#include <cmath>
#include <filesystem>
#include <fstream>

// VectorView class compatible with std::vector<float>
class VectorView {
public:
    VectorView() = default;
    VectorView(const float* data, size_t size, int64_t id = -1)
        : data_(data), size_(size), id_(id) {}

    // std::vector<float> compatible interface
    size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }

    const float& operator[](size_t i) const { return data_[i]; }
    const float& at(size_t i) const {
        if (i >= size_) throw std::out_of_range("Index out of range");
        return data_[i];
    }
    const float& front() const { return data_[0]; }
    const float& back() const { return data_[size_ - 1]; }

    const float* begin() const { return data_; }
    const float* end() const { return data_ + size_; }
    const float* cbegin() const { return data_; }
    const float* cend() const { return data_ + size_; }

    const float* data() const { return data_; }

    // Vector operations
    float dot_product(const VectorView& other) const {
        EXPECT_EQ(size_, other.size_);
        float sum = 0.0f;
        for (size_t i = 0; i < size_; ++i) {
            sum += data_[i] * other.data_[i];
        }
        return sum;
    }

    float l2_distance(const VectorView& other) const {
        EXPECT_EQ(size_, other.size_);
        float sum = 0.0f;
        for (size_t i = 0; i < size_; ++i) {
            float diff = data_[i] - other.data_[i];
            sum += diff * diff;
        }
        return std::sqrt(sum);
    }

    float l2_distance_square(const VectorView& other) const {
        EXPECT_EQ(size_, other.size_);
        float sum = 0.0f;
        for (size_t i = 0; i < size_; ++i) {
            float diff = data_[i] - other.data_[i];
            sum += diff * diff;
        }
        return sum;
    }

    int64_t id() const { return id_; }

private:
    const float* data_ = nullptr;
    size_t size_ = 0;
    int64_t id_ = -1;
};

// Helper: Write vectors to Parquet file
bool write_vectors_to_parquet(const std::string& path,
                              const std::vector<int64_t>& ids,
                              const std::vector<std::vector<float>>& vectors) {
    if (ids.size() != vectors.size()) return false;

    arrow::Int64Builder id_builder(arrow::default_memory_pool());
    arrow::ListBuilder list_builder(
        arrow::default_memory_pool(),
        std::make_shared<arrow::FloatBuilder>(arrow::default_memory_pool())
    );
    auto& float_builder = static_cast<arrow::FloatBuilder&>(*list_builder.value_builder());

    // Reserve space
    if (!id_builder.Reserve(ids.size()).ok()) return false;
    if (!list_builder.Reserve(ids.size()).ok()) return false;

    size_t total_floats = 0;
    for (const auto& vec : vectors) {
        total_floats += vec.size();
    }
    if (!float_builder.Reserve(total_floats).ok()) return false;

    // Append data
    for (size_t i = 0; i < ids.size(); ++i) {
        if (!id_builder.Append(ids[i]).ok()) return false;
        if (!list_builder.Append().ok()) return false;
        if (!float_builder.AppendValues(vectors[i].data(), vectors[i].size()).ok()) return false;
    }

    // Build arrays
    auto id_array_result = id_builder.Finish();
    auto vector_array_result = list_builder.Finish();
    if (!id_array_result.ok() || !vector_array_result.ok()) return false;

    // Create schema
    auto schema = arrow::schema({
        arrow::field("id", arrow::int64()),
        arrow::field("vector", arrow::list(arrow::float32()))
    });

    // Create table
    auto table = arrow::Table::Make(schema, {std::move(id_array_result).ValueOrDie(), std::move(vector_array_result).ValueOrDie()}, ids.size());

    // Write to file
    auto write_options = parquet::WriterProperties::Builder()
        .data_pagesize(sizeof(float) * total_floats)
        ->compression(parquet::Compression::UNCOMPRESSED)
        ->build();
    
    // Open file output stream
    std::shared_ptr<arrow::io::FileOutputStream> outstream;
    PARQUET_ASSIGN_OR_THROW(outstream,
                            arrow::io::FileOutputStream::Open(path));
    
    auto result = parquet::arrow::WriteTable(*table, arrow::default_memory_pool(), 
                                             outstream, 1024, write_options);
    return result.ok();
}

// Helper: Read vectors from Parquet file
class ParquetVectorStore {
public:
    bool load(const std::string& path) {
        auto result = arrow::io::ReadableFile::Open(path);
        if (!result.ok()) return false;
        auto file = std::move(result).ValueOrDie();

        auto reader_result = parquet::arrow::OpenFile(file, arrow::default_memory_pool());
        if (!reader_result.ok()) return false;
        auto reader = std::move(reader_result).ValueOrDie();

        std::shared_ptr<arrow::Table> table;
        if (!reader->ReadTable(&table).ok()) return false;
        table_ = table;

        // Get columns
        id_column_ = std::static_pointer_cast<arrow::Int64Array>(table_->column(0)->chunk(0));
        vector_column_ = std::static_pointer_cast<arrow::ListArray>(table_->column(1)->chunk(0));

        // Build id to index map
        id_to_index_.clear();
        for (int64_t i = 0; i < id_column_->length(); ++i) {
            id_to_index_[id_column_->Value(i)] = i;
        }

        // Get dimension from first vector
        if (vector_column_->length() > 0) {
            auto first_value = std::static_pointer_cast<arrow::FloatArray>(vector_column_->value_slice(0));
            dimension_ = first_value->length();
        }

        return true;
    }

    VectorView operator[](int64_t id) const {
        auto it = id_to_index_.find(id);
        if (it == id_to_index_.end()) {
            return VectorView(nullptr, 0, -1);
        }

        int64_t idx = it->second;
        auto value_array = std::static_pointer_cast<arrow::FloatArray>(vector_column_->value_slice(idx));

        return VectorView(value_array->raw_values(), value_array->length(), id);
    }

    VectorView at(int64_t idx) const {
        if (idx < 0 || idx >= id_column_->length()) {
            throw std::out_of_range("Index out of range");
        }
        int64_t id = id_column_->Value(idx);
        auto value_array = std::static_pointer_cast<arrow::FloatArray>(vector_column_->value_slice(idx));
        return VectorView(value_array->raw_values(), value_array->length(), id);
    }

    std::vector<VectorView> batch_get(const std::vector<int64_t>& ids) const {
        std::vector<VectorView> results;
        results.reserve(ids.size());
        for (int64_t id : ids) {
            results.push_back((*this)[id]);
        }
        return results;
    }

    size_t size() const { return id_column_->length(); }
    bool empty() const { return size() == 0; }
    size_t dimension() const { return dimension_; }

    // Iterator support
    class Iterator {
    public:
        Iterator(const ParquetVectorStore* store, int64_t index) : store_(store), index_(index) {
            update_view();
        }

        Iterator& operator++() {
            ++index_;
            update_view();
            return *this;
        }

        bool operator!=(const Iterator& other) const {
            return index_ != other.index_;
        }

        const VectorView& operator*() const { return current_view_; }

    private:
        void update_view() {
            if (index_ < store_->id_column_->length()) {
                int64_t id = store_->id_column_->Value(index_);
                auto value_array = std::static_pointer_cast<arrow::FloatArray>(store_->vector_column_->value_slice(index_));
                current_view_ = VectorView(value_array->raw_values(), value_array->length(), id);
            }
        }

        const ParquetVectorStore* store_;
        int64_t index_;
        VectorView current_view_;
    };

    Iterator begin() const { return Iterator(this, 0); }
    Iterator end() const { return Iterator(this, id_column_->length()); }

private:
    std::shared_ptr<arrow::Table> table_;
    std::shared_ptr<arrow::Int64Array> id_column_;
    std::shared_ptr<arrow::ListArray> vector_column_;
    std::unordered_map<int64_t, int64_t> id_to_index_;
    size_t dimension_ = 0;
};

// Test fixture
class ArrowParquetTest : public ::testing::Test {
protected:
    std::string test_file_ = "test_vectors.parquet";

    void SetUp() override {
        // Clean up any existing test file
        std::filesystem::remove(test_file_);
    }

    void TearDown() override {
        // Clean up test file
        std::filesystem::remove(test_file_);
    }
};

// Test: Write and read Parquet file
TEST_F(ArrowParquetTest, WriteAndReadParquet) {
    std::vector<int64_t> ids = {0, 1, 2, 3, 4};
    std::vector<std::vector<float>> vectors = {
        {0.1f, 0.2f, 0.3f, 0.4f},
        {0.5f, 0.6f, 0.7f, 0.8f},
        {0.9f, 1.0f, 1.1f, 1.2f},
        {1.3f, 1.4f, 1.5f, 1.6f},
        {1.7f, 1.8f, 1.9f, 2.0f}
    };

    // Write to Parquet
    EXPECT_TRUE(write_vectors_to_parquet(test_file_, ids, vectors));

    // Read from Parquet
    ParquetVectorStore store;
    EXPECT_TRUE(store.load(test_file_));

    // Verify
    EXPECT_EQ(store.size(), 5);
    EXPECT_EQ(store.dimension(), 4);

    for (size_t i = 0; i < ids.size(); ++i) {
        VectorView vec = store[ids[i]];
        EXPECT_EQ(vec.size(), 4);
        EXPECT_EQ(vec.id(), ids[i]);

        for (size_t j = 0; j < 4; ++j) {
            EXPECT_FLOAT_EQ(vec[j], vectors[i][j]);
        }
    }
}

// Test: VectorView interface compatibility with std::vector<float>
TEST_F(ArrowParquetTest, VectorViewStdCompatibility) {
    std::vector<int64_t> ids = {0, 1};
    std::vector<std::vector<float>> vectors = {
        {0.1f, 0.2f, 0.3f, 0.4f},
        {0.5f, 0.6f, 0.7f, 0.8f}
    };

    EXPECT_TRUE(write_vectors_to_parquet(test_file_, ids, vectors));

    ParquetVectorStore store;
    EXPECT_TRUE(store.load(test_file_));

    VectorView vec = store[0];

    // Test size and empty
    EXPECT_EQ(vec.size(), 4);
    EXPECT_FALSE(vec.empty());

    // Test element access
    EXPECT_FLOAT_EQ(vec[0], 0.1f);
    EXPECT_FLOAT_EQ(vec.at(1), 0.2f);
    EXPECT_FLOAT_EQ(vec.front(), 0.1f);
    EXPECT_FLOAT_EQ(vec.back(), 0.4f);

    // Test iterator (range-based for)
    int i = 0;
    for (float val : vec) {
        EXPECT_FLOAT_EQ(val, vectors[0][i]);
        ++i;
    }

    // Test data pointer
    const float* ptr = vec.data();
    EXPECT_FLOAT_EQ(ptr[0], 0.1f);
    EXPECT_FLOAT_EQ(ptr[1], 0.2f);
}

// Test: Vector operations (dot product and L2 distance)
TEST_F(ArrowParquetTest, VectorOperations) {
    std::vector<int64_t> ids = {0, 1};
    std::vector<std::vector<float>> vectors = {
        {1.0f, 2.0f, 3.0f},
        {4.0f, 5.0f, 6.0f}
    };

    EXPECT_TRUE(write_vectors_to_parquet(test_file_, ids, vectors));

    ParquetVectorStore store;
    EXPECT_TRUE(store.load(test_file_));

    VectorView v1 = store[0];
    VectorView v2 = store[1];

    // Test dot product: 1*4 + 2*5 + 3*6 = 4 + 10 + 18 = 32
    float dot = v1.dot_product(v2);
    EXPECT_FLOAT_EQ(dot, 32.0f);

    // Test L2 distance squared: (1-4)^2 + (2-5)^2 + (3-6)^2 = 9 + 9 + 9 = 27
    float l2_sq = v1.l2_distance_square(v2);
    EXPECT_FLOAT_EQ(l2_sq, 27.0f);

    // Test L2 distance: sqrt(27) = 5.196152...
    float l2 = v1.l2_distance(v2);
    EXPECT_FLOAT_EQ(l2, std::sqrt(27.0f));
}

// Test: Batch get operation
TEST_F(ArrowParquetTest, BatchGet) {
    std::vector<int64_t> ids = {0, 1, 2, 3, 4};
    std::vector<std::vector<float>> vectors = {
        {0.1f, 0.2f, 0.3f, 0.4f},
        {0.5f, 0.6f, 0.7f, 0.8f},
        {0.9f, 1.0f, 1.1f, 1.2f},
        {1.3f, 1.4f, 1.5f, 1.6f},
        {1.7f, 1.8f, 1.9f, 2.0f}
    };

    EXPECT_TRUE(write_vectors_to_parquet(test_file_, ids, vectors));

    ParquetVectorStore store;
    EXPECT_TRUE(store.load(test_file_));

    std::vector<int64_t> query_ids = {0, 2, 4};
    auto batch = store.batch_get(query_ids);

    EXPECT_EQ(batch.size(), 3);

    for (size_t i = 0; i < query_ids.size(); ++i) {
        EXPECT_EQ(batch[i].id(), query_ids[i]);
        EXPECT_EQ(batch[i].size(), 4);
    }
}

// Test: Iterator support
TEST_F(ArrowParquetTest, IteratorSupport) {
    std::vector<int64_t> ids = {10, 20, 30};
    std::vector<std::vector<float>> vectors = {
        {1.0f, 2.0f},
        {3.0f, 4.0f},
        {5.0f, 6.0f}
    };

    EXPECT_TRUE(write_vectors_to_parquet(test_file_, ids, vectors));

    ParquetVectorStore store;
    EXPECT_TRUE(store.load(test_file_));

    // Test iterator
    int count = 0;
    for (const auto& vec : store) {
        EXPECT_EQ(vec.size(), 2);
        EXPECT_EQ(vec.id(), ids[count]);
        ++count;
    }
    EXPECT_EQ(count, 3);
}

// Test: Non-existent ID
TEST_F(ArrowParquetTest, NonExistentId) {
    std::vector<int64_t> ids = {0, 1, 2};
    std::vector<std::vector<float>> vectors = {
        {1.0f, 2.0f},
        {3.0f, 4.0f},
        {5.0f, 6.0f}
    };

    EXPECT_TRUE(write_vectors_to_parquet(test_file_, ids, vectors));

    ParquetVectorStore store;
    EXPECT_TRUE(store.load(test_file_));

    // Access non-existent ID
    VectorView vec = store[999];
    EXPECT_TRUE(vec.empty());
    EXPECT_EQ(vec.id(), -1);
    EXPECT_EQ(vec.data(), nullptr);
}

// Test: Large vector set (performance test)
TEST_F(ArrowParquetTest, LargeVectorSet) {
    const size_t num_vectors = 10000;
    const size_t dimension = 128;

    std::vector<int64_t> ids;
    std::vector<std::vector<float>> vectors;

    ids.reserve(num_vectors);
    vectors.reserve(num_vectors);

    for (size_t i = 0; i < num_vectors; ++i) {
        ids.push_back(static_cast<int64_t>(i));
        std::vector<float> vec(dimension);
        for (size_t j = 0; j < dimension; ++j) {
            vec[j] = static_cast<float>(i * dimension + j) / 1000.0f;
        }
        vectors.push_back(std::move(vec));
    }

    EXPECT_TRUE(write_vectors_to_parquet(test_file_, ids, vectors));

    ParquetVectorStore store;
    EXPECT_TRUE(store.load(test_file_));

    EXPECT_EQ(store.size(), num_vectors);
    EXPECT_EQ(store.dimension(), dimension);

    // Sample check
    for (size_t i = 0; i < 100; ++i) {
        size_t idx = i * 100;
        VectorView vec = store[static_cast<int64_t>(idx)];
        EXPECT_EQ(vec.size(), dimension);
        EXPECT_EQ(vec.id(), static_cast<int64_t>(idx));
        EXPECT_FLOAT_EQ(vec[0], vectors[idx][0]);
        EXPECT_FLOAT_EQ(vec[dimension - 1], vectors[idx][dimension - 1]);
    }
}

// Main
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
