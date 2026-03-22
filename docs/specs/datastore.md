# DataStore接口
## 作用
- 主要负责索引和向量数据文件的文件的获取

## 主要接口
```c++
class DataStore {
public:
    // 写入数据
    size_t Write(const std::string& path, const void* data, size_t size);
    
    // 从开头读取
    size_t Read(const std::string& path, void* buffer, size_t size);
    
    // 从指定 offset 读取
    size_t ReadAt(const std::string& path, size_t offset, void* buffer, size_t size);
    
    // 检查文件是否存在
    bool Exists(const std::string& path);
    
    // 删除文件
    bool Delete(const std::string& path);
};
```