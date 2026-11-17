#pragma once
#include <libcamera/camera_manager.h>
#include <libcamera/request.h>
#include <memory>
#include <opencv2/calib3d.hpp>
#include <libcamera/camera.h>
#include "../include/common.h"
#include <libcamera/framebuffer_allocator.h>
#include <unordered_map>
#include <sys/mman.h>
#include "../include/thread_pool.h"
#include <atomic>

class CameraTaskManger
{
public:
    unsigned int g_width, g_height, g_stride;
public:
    bool open();
    bool setConfigure(StreamSetting& s);
    void setFrameCallback(std::function<void(const Frame&)> cb); // 複製給顯示用(live，之後還有grab和snap)
    bool framebufferAllocator();
    bool bindingRequest();
    void start();
    void stop();
    ~CameraTaskManger();
private:
    void onRequestComplete(libcamera::Request* req); // callback function
    void processRequest(libcamera::Request* request);
private:
    
    std::atomic<bool> running{false};
    std::function<void(const Frame&)> _frameCallback;
    std::unique_ptr<libcamera::CameraManager> _cm;
    std::shared_ptr<libcamera::Camera> _camera;
    std::unique_ptr<libcamera::CameraConfiguration> _config;
    std::unique_ptr<libcamera::FrameBufferAllocator> _allocator;
    std::vector<std::unique_ptr<libcamera::Request>> requests;
    std::unordered_map<libcamera::FrameBuffer*, Mapping> unmap;
    TaskManager taskManager;
};