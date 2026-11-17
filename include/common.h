#pragma once
#include <cstdint>
#include <libcamera/formats.h>
#include <libcamera/pixel_format.h>
#include <opencv2/calib3d.hpp>
struct FrameInfo {
    uint32_t sequence = 0;
    uint64_t timestamp_ns = 0;
};
struct Frame 
{
    const uint8_t* data;
    unsigned int width, height, stride;
    FrameInfo info;
};
struct OwnedFrame {
    cv::Mat image;
    FrameInfo info;
};
// 包含多種stream的參數
struct StreamSetting
{
    unsigned int width = 1920;
    unsigned int height = 1080;
    unsigned int buffercount = 5;
    libcamera::PixelFormat format = libcamera::formats::RGB888;
};
struct Mapping{uint8_t* pixels; void* base; size_t size;};