#include <gtest/gtest.h>
#include <vector>
#include <memory>

// Simple test to verify gtest integration
TEST(GTestIntegrationTest, BasicAssertions) {
    // Test that gtest is working
    EXPECT_EQ(1, 1);
    EXPECT_TRUE(true);
    EXPECT_FALSE(false);
}

TEST(GTestIntegrationTest, VectorOperations) {
    // Test basic vector operations
    std::vector<int> vec = {1, 2, 3, 4, 5};
    
    EXPECT_EQ(vec.size(), 5);
    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec[4], 5);
    
    vec.push_back(6);
    EXPECT_EQ(vec.size(), 6);
    EXPECT_EQ(vec[5], 6);
}

TEST(GTestIntegrationTest, StringOperations) {
    // Test string operations
    std::string str = "Hello, World!";
    
    EXPECT_EQ(str.length(), 13);
    EXPECT_TRUE(str.find("Hello") != std::string::npos);
    EXPECT_FALSE(str.find("xyz") != std::string::npos);
}

class TestFixture : public ::testing::Test {
protected:
    void SetUp() override {
        data_ = std::make_unique<std::vector<int>>(std::vector<int>{10, 20, 30});
    }
    
    void TearDown() override {
        data_.reset();
    }
    
    std::unique_ptr<std::vector<int>> data_;
};

TEST_F(TestFixture, FixtureTest) {
    ASSERT_NE(data_, nullptr);
    EXPECT_EQ(data_->size(), 3);
    EXPECT_EQ((*data_)[0], 10);
    EXPECT_EQ((*data_)[2], 30);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
