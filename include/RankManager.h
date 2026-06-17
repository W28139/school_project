#ifndef RANK_MANAGER_H
#define RANK_MANAGER_H

#include "Video.h"
#include <vector>
// #include <unordered_map>
#include<MyHashMap.h>
#include <memory>
#include <mutex>

class RankManager {
public:
    RankManager(int topK = 100);
    
    // 更新或新增视频互动数据
    void updateVideo(const std::string& id, int type, double value);
    
    // 获取当前 Top100 榜单
    std::vector<std::shared_ptr<Video>> getTopList();
    
    void syncToDb();

    void saveToFile(const std::string& filename);
    void loadFromFile(const std::string& filename);

    void set_W(double w1,double w2,double w3,double w4);
    
private:
    int _topK;
    // 哈希表：ID -> 视频对象指针
    MyHashMap<std::string, std::shared_ptr<Video>> _videoMap;
    
    // 小顶堆：存储热度最高的 K 个视频
    // 注意：维护 TopK 高热度，堆顶应该是这 K 个里最小的，这样新来的大的才能顶替它
    std::vector<std::shared_ptr<Video>> _minHeap;

    // 内部调整堆的方法
    void heapifyUp(int index);
    void heapifyDown(int index);
    
    // 权重系数
    double _w1 = 0.4, _w2 = 0.3, _w3 = 0.2, _w4 = 0.1;

    std::mutex _mutex;

};

#endif