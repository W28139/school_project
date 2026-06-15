#include "RankManager.h"
#include <fstream>
#include <algorithm>
#include <iostream>
#include <mysql/mysql.h>
#include "SqlConnRAII.h"

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
    if (_videoMap.find(id) == _videoMap.end()) 
    {
        _videoMap[id] = std::make_shared<Video>(id);
    }
    auto video = _videoMap[id];

    // 2. 更新对应指标
    switch (type) 
    {
        case 1: video->playCount += (long)value; break; // 播放
        case 2: video->likeCount += (long)value; break; // 点赞
        case 3: video->shareCount += (long)value; break; // 分享
        case 4: video->finishRate = value; break;        // 完播率更新
        default: break;
    }

    // 3. 重新计算热度
    double oldHeat = video->heat;
    video->calculateHeat(_w1, _w2, _w3, _w4);

    // 4. 维护小顶堆 (Top 100)
    // 情况A：视频已在榜单中
    if (video->heapIndex != -1)
    {
        // 由于是增加热度，在小顶堆中，热度变大应该向下调整（远离堆顶）
        // 如果热度变小（虽然本场景较少），则向上调整
        if(video->heat > oldHeat)
        {
            heapifyDown(video->heapIndex);
        }
        else 
        {
            heapifyUp(video->heapIndex);
        }
    }
    // 情况B：视频不在榜单中，且榜单未满
    else if (_minHeap.size() < _topK)
    {
        video->heapIndex = (int)_minHeap.size();
        _minHeap.push_back(video);
        heapifyUp(video->heapIndex);
    }
    // 情况C：视频不在榜单中，且榜单已满，对比堆顶（第100名）
    else if (video->heat > _minHeap[0]->heat) 
    {
        // 替换掉原来的最后一名（堆顶）
        _minHeap[0]->heapIndex = -1; // 踢出榜单
        _minHeap[0] = video;
        video->heapIndex = 0;
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
    for (auto& pair : _videoMap) {
        auto v = pair.second;
        // 保存 ID 播放量 点赞数 分享数 完播率
        ofs << v->videoId << " " << v->playCount << " " << v->likeCount << " " 
            << v->shareCount << " " << v->finishRate << "\n";
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
    // 1. 锁定内存数据
    std::vector<std::shared_ptr<Video>> topList;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        topList = _minHeap; // 我们同步当前在榜单上的视频即可
    }

    if(topList.empty()) return;

    // 2. 从池中获取连接
    MYSQL* sql = nullptr;
    SqlConnRAII(&sql, SqlConnPool::Instance());
    
    // 3. 批量写入优化：开启事务
    mysql_query(sql, "START TRANSACTION");

    for (auto& v : topList) {
        char sqlStr[512];
        // 使用 ON DUPLICATE KEY UPDATE：如果 ID 存在就更新，不存在就插入
        sprintf(sqlStr, "INSERT INTO t_video_rank (video_id, play_count, like_count, heat) "
                        "VALUES ('%s', %ld, %ld, %f) ON DUPLICATE KEY UPDATE "
                        "play_count=%ld, like_count=%ld, heat=%f",
                v->videoId.c_str(), v->playCount, v->likeCount, v->heat,
                v->playCount, v->likeCount, v->heat);
        
        if (mysql_query(sql, sqlStr)) {
            std::cerr << "MySQL Update Error: " << mysql_error(sql) << std::endl;
        }
    }
    
    mysql_query(sql, "COMMIT"); // 提交事务
    // std::cout << "Successfully synced " << topList.size() << " videos to MySQL." << std::endl;
}