#include "RankManager.h"
#include <fstream>
#include <algorithm>
#include <iostream>
#include <mysql/mysql.h>
#include "CommonConnectionPool.h"

// #include "SqlConnRAII.h"
RankManager::RankManager(int topK) : _topK(topK) {
    _minHeap.reserve(_topK);
    // 显式初始化权重，否则 heat 计算会出错
    _w1 = 0.4; _w2 = 0.3; _w3 = 0.2; _w4 = 0.1;
}

// 核心方法：更新视频数据并调整榜单
void RankManager::updateVideo(const std::string& id, int type, double value) 
{
    std::lock_guard<std::mutex> lock(_mutex);
    // 1. 获取或创建视频对象
    std::shared_ptr<Video>* vPtr = _videoMap.get(id);

    if (vPtr == nullptr) {
        // 如果不存在，创建并存入
        auto newVideo = std::make_shared<Video>(id);
        _videoMap.put(id, newVideo);
        vPtr = _videoMap.get(id); // 重新获取指针
    }

    // 这里 v 是 std::shared_ptr<Video> 的引用
    auto& v = *vPtr;

    // 2. 更新对应指标
    switch (type) 
    {
        case 1: v->playCount += (long)value; break; // 播放
        case 2: v->likeCount += (long)value; break; // 点赞
        case 3: v->shareCount += (long)value; break; // 分享
        case 4: v->finishRate = value; break;        // 完播率更新
        default: break;
    }

    // 3. 重新计算热度
    double oldHeat = v->heat;
    v->calculateHeat(_w1, _w2, _w3, _w4);

    // 4. 维护小顶堆 (Top 100)
    // 情况A：视频已在榜单中
    if (v->heapIndex != -1)
    {
        // 由于是增加热度，在小顶堆中，热度变大应该向下调整（远离堆顶）
        // 如果热度变小（虽然本场景较少），则向上调整
        if(v->heat > oldHeat)
        {
            heapifyDown(v->heapIndex);
        }
        else 
        {
            heapifyUp(v->heapIndex);
        }
    }
    // 情况B：视频不在榜单中，且榜单未满
    else if (_minHeap.size() < _topK)
    {
        v->heapIndex = (int)_minHeap.size();
        _minHeap.push_back(v);
        heapifyUp(v->heapIndex);
    }
    // 情况C：视频不在榜单中，且榜单已满，对比堆顶（第100名）
    else if (v->heat > _minHeap[0]->heat) 
    {
        // 替换掉原来的最后一名（堆顶）
        _minHeap[0]->heapIndex = -1; // 踢出榜单
        _minHeap[0] = v;
        v->heapIndex = 0;
        heapifyDown(0);
    }
}

/*
 * 上滤操作：将节点向上移动，直到满足小顶堆性质（父节点热度 <= 子节点热度）
 * @param index 需要向上调整的节点在 _minHeap 数组中的下标
 */
void RankManager::heapifyUp(int index)
{
    // 如果 index 为 0，说明已经到达堆顶，无法再向上调整
    while (index > 0) 
    {
        // 根据完全二叉树公式计算父节点下标：parent = (i - 1) / 2
        int parent = (index - 1) / 2;

        // 小顶堆性质：父节点热度应该比子节点小
        // 如果当前节点热度 < 父节点热度，说明顺序反了，需要交换
        if (_minHeap[index]->heat < _minHeap[parent]->heat) 
        {
            // --- 核心同步步骤 ---
            // 1. 同步更新视频对象内部记录的堆下标（非常重要！）
            // 交换后，原 index 处的视频去了 parent 位置，反之亦然
            std::swap(_minHeap[index]->heapIndex, _minHeap[parent]->heapIndex);

            // 2. 交换堆数组中存储的智能指针
            std::swap(_minHeap[index], _minHeap[parent]);

            // --- 继续迭代 ---
            // 将当前处理的下标更新为父节点下标，继续向上比较
            index = parent;
        } 
        else 
        {
            // 如果当前节点热度已经 >= 父节点热度，说明已经满足小顶堆性质，停止调整
            break;
        }
    }
}

/**
 * 下滤操作：将节点向下移动，直到满足小顶堆性质
 * @param index 需要向下调整的节点在 _minHeap 数组中的下标
 */
void RankManager::heapifyDown(int index) 
{
    int size = (int)_minHeap.size();
    
    // 开启死循环，直到找到合适位置或到达叶子节点
    while (true) 
    {
        int smallest = index;      // 假设当前节点是三者（父、左、右）中热度最小的
        int left = 2 * index + 1;  // 计算左孩子下标
        int right = 2 * index + 2; // 计算右孩子下标

        // 步骤1：检查左孩子
        // 如果左孩子存在，且左孩子热度比当前记录的 smallest 还要小
        if (left < size && _minHeap[left]->heat < _minHeap[smallest]->heat)
        {
            smallest = left; // 更新最小热度目标的下标
        }

        // 步骤2：检查右孩子
        // 如果右孩子存在，且右孩子热度比当前记录的 smallest（可能是父或左）还要小
        if (right < size && _minHeap[right]->heat < _minHeap[smallest]->heat)
        {
            smallest = right; // 更新最小热度目标的下标
        }

        // 步骤3：判断是否需要交换
        // 如果 smallest 不再是 index，说明子节点中有更小的（更弱的热度）
        if (smallest != index) 
        {
            // --- 核心同步步骤 ---
            // 1. 同步更新两个视频对象内部的堆下标记录，确保哈希表定位依然准确定位
            std::swap(_minHeap[index]->heapIndex, _minHeap[smallest]->heapIndex);
    
            // 2. 交换堆数组中的指针位置
            std::swap(_minHeap[index], _minHeap[smallest]);

            // --- 继续迭代 ---
            // 更新 index 为下沉后的新位置，继续向下层比较
            index = smallest;
        } 
        else 
        {
            // 如果 smallest 就是 index 本身，说明它比两个孩子都小，调整完成
            break;
        }
    }
}

// 获取 Top 榜单并按热度降序排列
std::vector<std::shared_ptr<Video>> RankManager::getTopList() 
{
    std::vector<std::shared_ptr<Video>> result;

    {
        std::lock_guard<std::mutex> lock(_mutex);
        result = _minHeap;
    }

    std::sort(
        result.begin(),
        result.end(),
        [](const auto& a,const auto& b)
        {
            return a->heat > b->heat;
        }
    );

    return result;
}

void RankManager::saveToFile(const std::string& filename) {
    std::lock_guard<std::mutex> lock(_mutex);
    std::ofstream ofs(filename);
    
    // 1. 获取所有的桶 (vector)
    auto& buckets = _videoMap.getBuckets();

    // 2. 遍历每一个桶
    for (auto& bucket : buckets) {
        // 3. 遍历桶里的链表 (list)
        for (auto& node : bucket) {
            // node.key 是 ID, node.value 是 shared_ptr<Video>
            auto& v = node.value;
            
            // 保存 ID 播放量 点赞数 分享数 完播率
            ofs << v->videoId << " " << v->playCount << " " << v->likeCount << " " 
                << v->shareCount << " " << v->finishRate << "\n";
        }
    }
}

void RankManager::loadFromFile(const std::string& filename) {
    std::ifstream ifs(filename);
    if (!ifs.is_open()) return;
    
    std::string id;
    long play, like, share;
    double rate;
    while (ifs >> id >> play >> like >> share >> rate) {
        // 这里可以直接调用之前的逻辑重新构建内存数据
        updateVideo(id, 1, play);
        updateVideo(id, 2, like);
        updateVideo(id, 3, share);
        updateVideo(id, 4, rate);
    }
}


void RankManager::syncToDb() {
    // 1. 锁定并拷贝内存数据（保持原样）
    std::vector<std::shared_ptr<Video>> topList;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        topList = _minHeap; 
    }

    if(topList.empty()) return;

    // 2. 从新的连接池中获取连接
    // getConnection() 返回的是 std::shared_ptr<Connection>
    auto sp = ConnectionPool::getConnectionPool()->getConnection();
    if (sp == nullptr) {
        LOG("连接池为空，请检查配置文件是否在当前目录下！");
        return; 
    }
    
    if (sp == nullptr) {
        LOG("获取数据库连接失败，无法同步数据");
        return;
    }
    
    // 3. 批量写入优化：开启事务
    // 直接使用封装好的 update 方法执行 SQL
    sp->update("START TRANSACTION");

    for (auto& v : topList) {
        char sqlStr[1024]; // 建议稍微大一点，防止 video_id 过长
        
        // 使用 snprintf 代替 sprintf 更安全
        snprintf(sqlStr, sizeof(sqlStr), 
                "INSERT INTO t_video_rank (video_id, play_count, like_count, heat) "
                "VALUES ('%s', %ld, %ld, %f) ON DUPLICATE KEY UPDATE "
                "play_count=%ld, like_count=%ld, heat=%f",
                v->videoId.c_str(), v->playCount, v->likeCount, v->heat,
                v->playCount, v->likeCount, v->heat);
        
        // 执行更新
        if (!sp->update(sqlStr)) {
            // 如果某一条失败，Connection 内部已经打印了 LOG
            // 这里可以选择是否继续或回滚
        }
    }
    
    // 提交事务
    sp->update("COMMIT"); 
    
    // 当 sp 离开作用域，智能指针析构，连接会自动归还到连接池队列中
}