#pragma once
#include <vector>
#include <list>
#include <string>
#include <algorithm>
#include <functional>

/**
 * @brief 自定义哈希表模板类
 * @tparam K 键类型（需要支持 std::hash）
 * @tparam V 值类型
 */
template <typename K, typename V>
class MyHashMap {
private:
    // --- 内部数据结构 ---
    
    /**
     * @brief 存储节点，保存具体的键值对
     */
    struct Node {
        K key;
        V value;
        Node(K k, V v) : key(k), value(v) {}
    };

    // --- 成员变量 ---
    
    // 桶数组：每个桶是一个 std::list（双向链表），用于处理哈希冲突
    // 采用 vector 动态管理桶的数量
    std::vector<std::list<Node>> _buckets; 
    
    size_t _size;                          // 哈希表中当前存储的元素总数
    const double _maxLoadFactor = 0.75;    // 最大负载因子：当 元素数/桶数 超过此值时触发扩容

    /**
     * @brief 计算键在当前桶数组中的索引下标
     * @param key 键
     * @param bucketCount 当前桶的总数
     * @return 映射后的数组下标
     */
    size_t getBucketIndex(const K& key, size_t bucketCount) const {
        // 使用标准库的哈希函数对象进行哈希化，然后取模对齐到数组长度
        return std::hash<K>{}(key) % bucketCount;
    }

    /**
     * @brief 扩容函数：当负载因子过高时调用，保证查询效率接近 O(1)
     */
    void rehash() {
        // 1. 桶数量翻倍
        size_t newBucketCount = _buckets.size() * 2;
        // 2. 创建一个新的更大容量的桶数组
        std::vector<std::list<Node>> newBuckets(newBucketCount);

        // 3. 遍历旧桶数组中的每一个节点，重新映射到新数组中
        for (auto& bucket : _buckets) {
            for (auto& node : bucket) {
                // 根据新的桶数量计算新的下标
                size_t newIdx = getBucketIndex(node.key, newBucketCount);
                // 使用 std::move 转移数据，避免不必要的深拷贝，提高效率
                newBuckets[newIdx].push_back(std::move(node));
            }
        }
        // 4. 用新桶数组替换旧桶数组
        _buckets = std::move(newBuckets);
    }

public:
    /**
     * @brief 构造函数
     * @param initialCapacity 初始桶的数量，默认为 16
     */
    MyHashMap(size_t initialCapacity = 16) 
        : _buckets(initialCapacity), _size(0) {}

    /**
     * @brief 插入或更新键值对
     * @param key 键
     * @param value 值
     */
    void put(const K& key, const V& value) {
        // 检查负载因子，如果太拥挤则先扩容
        if ((double)_size / _buckets.size() > _maxLoadFactor) {
            rehash();
        }

        // 计算目标桶的下标
        size_t idx = getBucketIndex(key, _buckets.size());
        auto& bucket = _buckets[idx];

        // 遍历该链表，检查键是否已经存在
        for (auto& node : bucket) {
            if (node.key == key) {
                node.value = value; // 键已存在：更新值并返回
                return;
            }
        }

        // 键不存在：在链表末尾插入新节点
        // emplace_back 直接在容器内部构造对象，效率高于 push_back
        bucket.emplace_back(key, value);
        _size++;
    }

    /**
     * @brief 获取键对应的值
     * @param key 键
     * @return V* 指向值的指针。若不存在则返回 nullptr。
     * @note 返回指针比返回副本更高效，同时也方便外部直接修改值。
     */
    V* get(const K& key) {
        size_t idx = getBucketIndex(key, _buckets.size());
        auto& bucket = _buckets[idx];

        for (auto& node : bucket) {
            if (node.key == key) {
                return &node.value; // 找到键：返回其地址
            }
        }
        return nullptr; // 未找到
    }

    /**
     * @brief 检查哈希表中是否存在某个键
     */
    bool contains(const K& key) const {
        size_t idx = getBucketIndex(key, _buckets.size());
        for (const auto& node : _buckets[idx]) {
            if (node.key == key) return true;
        }
        return false;
    }

    /**
     * @brief 根据键删除元素
     * @param key 键
     */
    void remove(const K& key) {
        size_t idx = getBucketIndex(key, _buckets.size());
        auto& bucket = _buckets[idx];
        
        // 使用 std::remove_if 结合 lambda 表达式找到要删除的元素
        // remove_if 会将匹配的元素移到链表末尾，并返回指向第一个匹配元素的迭代器
        auto it = std::remove_if(bucket.begin(), bucket.end(), [&](const Node& n){
            return n.key == key;
        });

        // 如果找到了对应的键
        if (it != bucket.end()) {
            bucket.erase(it, bucket.end()); // 真正地从内存中删除节点
            _size--;
        }
    }

    /**
     * @brief 获取当前元素总数
     */
    size_t size() const { return _size; }

    /**
     * @brief 获取底层桶数组的引用
     * @return 用于外部遍历哈希表（例如 range-based for 循环）
     */
    auto& getBuckets() { return _buckets; }
};