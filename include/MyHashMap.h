#include <vector>
#include <list>
#include <string>
#include <algorithm>
#include <functional>

template <typename K, typename V>
class MyHashMap {
private:
    // 定义存储节点
    struct Node {
        K key;
        V value;
        Node(K k, V v) : key(k), value(v) {}
    };

    std::vector<std::list<Node>> _buckets; // 桶数组
    size_t _size;                          // 当前元素个数
    const double _maxLoadFactor = 0.75;    // 最大负载因子

    // 计算哈希下标
    size_t getBucketIndex(const K& key, size_t bucketCount) const {
        return std::hash<K>{}(key) % bucketCount;
    }

    // 扩容函数
    void rehash() {
        size_t newBucketCount = _buckets.size() * 2;
        std::vector<std::list<Node>> newBuckets(newBucketCount);

        for (auto& bucket : _buckets) {
            for (auto& node : bucket) {
                size_t newIdx = getBucketIndex(node.key, newBucketCount);
                newBuckets[newIdx].push_back(std::move(node));
            }
        }
        _buckets = std::move(newBuckets);
    }

public:
    MyHashMap(size_t initialCapacity = 16) 
        : _buckets(initialCapacity), _size(0) {}

    // 插入或更新
    void put(const K& key, const V& value) {
        if ((double)_size / _buckets.size() > _maxLoadFactor) {
            rehash();
        }

        size_t idx = getBucketIndex(key, _buckets.size());
        auto& bucket = _buckets[idx];

        // 检查键是否存在，存在则更新
        for (auto& node : bucket) {
            if (node.key == key) {
                node.value = value;
                return;
            }
        }

        // 不存在则插入新节点
        bucket.emplace_back(key, value);
        _size++;
    }

    // 获取值 (返回指针，找不到返回 nullptr)
    V* get(const K& key) {
        size_t idx = getBucketIndex(key, _buckets.size());
        auto& bucket = _buckets[idx];

        for (auto& node : bucket) {
            if (node.key == key) {
                return &node.value;
            }
        }
        return nullptr;
    }

    // 检查是否存在
    bool contains(const K& key) const {
        size_t idx = getBucketIndex(key, _buckets.size());
        for (const auto& node : _buckets[idx]) {
            if (node.key == key) return true;
        }
        return false;
    }

    // 删除
    void remove(const K& key) {
        size_t idx = getBucketIndex(key, _buckets.size());
        auto& bucket = _buckets[idx];
        
        auto it = std::remove_if(bucket.begin(), bucket.end(), [&](const Node& n){
            return n.key == key;
        });

        if (it != bucket.end()) {
            bucket.erase(it, bucket.end());
            _size--;
        }
    }

    size_t size() const { return _size; }

    // 为了支持 range-based for 循环 (简化版实现)
    // 实际项目中建议实现完整的 Iterator 类
    auto& getBuckets() { return _buckets; }
};