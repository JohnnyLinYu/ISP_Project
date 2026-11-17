#include <functional>
#include <iostream>
#include <libcamera/request.h>
#include "../include/CameraControl.h"

bool CameraTaskManger::open()
{
    _cm = std::make_unique<libcamera::CameraManager>();
    _cm->start();

    // 檢查是否有相機
    if(_cm->cameras().empty())
    {
       std::cout << "No cameras were identified on the system." << std::endl;
       _cm->stop();
       return false;
    }
    else 
    {
        std::cout << "Camera founded successfully." << std::endl;
    }

    // 鎖定相機給自己用
    _camera = _cm->cameras().front();
    int acqflag = _camera->acquire();
    if(acqflag < 0)
    {
        std::cout << "Acquired camera fail: " << acqflag << std::endl;
        _camera.reset();
        _cm->stop();
        return false;
    }
    std::cout << "Acquire camera success." << std::endl;
    return true;
}

// 這裡才會綁定config，多stream要調整
bool CameraTaskManger::setConfigure(StreamSetting& s)
{
    _config = _camera->generateConfiguration({libcamera::StreamRole::Viewfinder});
    _config->at(0).size.width = s.width;
    _config->at(0).size.height = s.height;
    _config->at(0).bufferCount = s.buffercount;
    _config->at(0).pixelFormat = s.format;
    auto status = _config->validate();
    if(status == libcamera::CameraConfiguration::Invalid)
    {
        return false;
    }
    g_width = _config->at(0).size.width;
    g_height = _config->at(0).size.height;
    g_stride = _config->at(0).stride;

    int ret = _camera->configure(_config.get());
    if(ret < 0)
    {
        std::cerr << "Camera setting failure." << std::endl;
        return false; 
    }

    return true;
}

// 之後有多個stream的時候要調整一下
bool CameraTaskManger::framebufferAllocator()
{
    _allocator = std::make_unique<libcamera::FrameBufferAllocator>(_camera);
    for(auto& cfg : *_config)
    {
        int ret = _allocator->allocate(cfg.stream()); // 請allocator為這條stream配置一組buffer
        if(ret < 0)
        {
            std::cerr << "Can't allocate buffers" << std::endl;
            return false;
        }
        // size_t allocated = _allocator->buffers(cfg.stream()).size();
        // std::cout << "Allocated " << allocated << " buffers for stream" << std::endl;
    }
    return true;
}

bool CameraTaskManger::bindingRequest()
{
    libcamera::Stream *stream = _config->at(0).stream();
    const std::vector<std::unique_ptr<libcamera::FrameBuffer>> &buffers = _allocator->buffers(stream); // 從某個stream中拿出buffer

    for(const auto& buf : _allocator->buffers(stream))
    {
        const auto& plane = buf->planes()[0];          // Bayer RAW 只有一個 plane
        void* mem = mmap(nullptr, plane.offset + plane.length,
                        PROT_READ, MAP_SHARED, plane.fd.get(), 0);
        unmap[buf.get()] = {static_cast<uint8_t*>(mem) + plane.offset, mem, plane.offset + plane.length};
    }

    for(int i = 0; i < buffers.size(); i++)
    {
        std::unique_ptr<libcamera::Request> request = _camera->createRequest();
        if(!request)
        {
            std::cerr << "Can't create request" << std::endl;
            return false;
        }

        const std::unique_ptr<libcamera::FrameBuffer> &buffer = buffers[i];
        int ret = request->addBuffer(stream, buffer.get()); // 這裡有綁定buffer，釋放的時候要注意

        if(ret < 0)
        {
            std::cerr << "Can't set buffer for request" << std::endl;
            return false;
        }
        requests.push_back(move(request));
    }

    std::cout << "Successfully binding request and FrameBuffer" << std::endl;
    return true;
}
//用來綁定LatestFrame
void CameraTaskManger::setFrameCallback(std::function<void(const Frame&)> cb)
{ 
    _frameCallback = std::move(cb);
}

void CameraTaskManger::start()
{
    _camera->requestCompleted.connect(this, &CameraTaskManger::onRequestComplete);
    running = true;
    _camera->start();

    for (std::unique_ptr<libcamera::Request> &request : requests)
        _camera->queueRequest(request.get());
}

void CameraTaskManger::stop()
{
    running = false;
    _camera->stop(); // 如果camera沒有start()，就不用stop()
    _camera->requestCompleted.disconnect(this, &CameraTaskManger::onRequestComplete);
    taskManager.stop();
}

CameraTaskManger::~CameraTaskManger()
{
    if(running)
    {
        stop();
    }
    for (auto& [buf, m] : unmap)                // 在 free 之前 munmap
        munmap(m.base, m.size);
    unmap.clear();

    requests.clear();
    _allocator.reset();
    _config.reset();
    _camera->release();
    _camera.reset();
    _cm->stop();
}

// private function
void CameraTaskManger::onRequestComplete(libcamera::Request* request)
{
    if(request->status() == libcamera::Request::RequestCancelled)
    {
        return;
    }
    std::cout << "One request completed!" << std::endl;
    
    taskManager.callLater([this, request](){processRequest(request);});
}
//多stream要調整
void CameraTaskManger::processRequest(libcamera::Request* request)
{
    const libcamera::Request::BufferMap &buffers = request->buffers();
    for(auto [stream, buffer] : buffers) // 多stream會比較多條
    {
        const libcamera::FrameMetadata &metadata = buffer->metadata();
        Frame f;
        f.data         = unmap.at(buffer).pixels;   // ← 從 Mapping 查出來
        f.width        = g_width;
        f.height       = g_height;
        f.stride       = g_stride;
        f.info.sequence     = metadata.sequence;                   // ← 從這一幀的 metadata
        f.info.timestamp_ns = metadata.timestamp;

        const auto planes = buffer->planes()[0]; // 不是YUV可以直接這樣
        int fd = planes.fd.get();
        size_t offset = planes.offset;
        size_t length = planes.length;
        
        if(_frameCallback)
            _frameCallback(f);
    }
    // 歸還request給相機
  
    if (running) {
        request->reuse(libcamera::Request::ReuseBuffers);
        _camera->queueRequest(request);
    }
}