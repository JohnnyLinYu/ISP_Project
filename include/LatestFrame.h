#pragma once
#include "../include/common.h"
#include <opencv2/highgui.hpp>
#include <condition_variable>
#include <mutex>
#include <cstring>
#include <iostream>
#include "common.h"
class LatestFrame
{
public:
    void init(unsigned int width, unsigned int height);
    void push(const Frame& f);
    bool waitAndCopy(OwnedFrame& of, std::chrono::milliseconds timeout);
private:
    std::mutex mtx;
    std::condition_variable cvar;
    cv::Mat display;
    bool _hasNew = false;
    FrameInfo _info;
};