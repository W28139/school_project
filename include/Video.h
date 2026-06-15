#ifndef VIDEO_H
#define VIDEO_H

#include <string>

struct Video {
    std::string videoId;
    long playCount;      // 播放量
    long likeCount;      // 点赞数
    long shareCount;     // 分享数
    double finishRate;   // 完播率
    double heat;         // 实时热度值
    int heapIndex=-1;       // 在堆中的下标（关键！用于更新已存在视频的热度）

    Video(std::string id) 
        : videoId(id), playCount(0), likeCount(0), 
          shareCount(0), finishRate(0.0), heat(0.0), heapIndex(-1) {}

    // 计算热度公式
    void calculateHeat(double w1, double w2, double w3, double w4) {
        heat = w1 * playCount + w2 * likeCount + w3 * shareCount + w4 * finishRate;
    }
};

#endif