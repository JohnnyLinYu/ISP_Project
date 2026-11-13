#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <libcamera/camera.h>
#include <libcamera/formats.h>
#include <libcamera/framebuffer.h>
#include <libcamera/framebuffer_allocator.h>
#include <libcamera/libcamera.h>
#include <libcamera/request.h>
#include <libcamera/stream.h>
#include <libcamera/control_ids.h>
#include <opencv2/core/hal/interface.h>
#include <opencv2/highgui.hpp>
#include <sys/mman.h>
#include <opencv2/opencv.hpp>
#include <fcntl.h>
#include <unistd.h>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <chrono>
#include <atomic>
#include "../include/thread_pool.h"
using namespace std;
using namespace libcamera;

void processRequest(Request *request);
static void requestComplete(Request* request);

// 全域參數
static mutex mtx;
static condition_variable cvar;
static bool shootOK = false;
int buffercount = 10; // 歷史回顧參數
int cnt = 0;
int frameCount = 0;
double fpsAvg = 0.0;

//相機參數
std::shared_ptr<Camera> camera;
static unsigned int gWidth = 0, gHeight = 0, gStride = 0;  

int twidth = 1920;
int theight = 1080;
int framebuffer = 5;
auto pixelformat = formats::RGB888;

struct Mapping{uint8_t* pixels; void* base; size_t size;};
unordered_map<libcamera::FrameBuffer*, Mapping>unmap;
static atomic<bool> running{false};    

// 影像local buffer
vector<cv::Mat> ImageBuffer(buffercount);
cv::Mat DisplayImage;

// Create Callback ThreadPool
TaskManager taskManager;

static void requestComplete(Request* request)
{
    if(request->status() == Request::RequestCancelled)
    {
        return;
    }
    cout << "One request completed!" << endl;
    
    taskManager.callLater([request](){processRequest(request);});
    // processRequest(request);
    

}

void processRequest(Request *request)
{
    const Request::BufferMap &buffers = request->buffers();
    for(auto [stream, buffer] : buffers) // 多stream會比較多條
    {
        const FrameMetadata &metadata = buffer->metadata();
        cout << "\n===== Frame Metadata =====" << endl;

        if(metadata.status != FrameMetadata::FrameSuccess)
        {
            cout << "Status: "
             << static_cast<int>(metadata.status) << endl;
            continue;
        }

        cout << "Sequence: " << metadata.sequence << endl;
        cout << "Timestamp (ns): " << metadata.timestamp << endl;
        for (size_t i = 0; i < metadata.planes().size(); ++i)
        {
            cout << "Plane " << i
                 << " Bytes Used: "
                 << metadata.planes()[i].bytesused
                 << endl;
        }
        
        const auto planes = buffer->planes()[0]; // 不是YUV可以直接這樣
        int fd = planes.fd.get();
        size_t offset = planes.offset;
        size_t length = planes.length;
        
        // void *base = mmap(nullptr, offset + length, PROT_READ, MAP_SHARED, fd, 0);
        // if(base == MAP_FAILED)
        // {
        //     cerr << "mmap" << endl;
        // }
        // else 
        // {
        //     uint8_t *imageData = static_cast<uint8_t*>(base) + offset;
        //     // 進行複製
        //     // memcpy(ImageBuffer[metadata.sequence].data, imageData, 1920*1080*3);
        //     memcpy(DisplayImage.data, unmap[buffer], twidth*theight*3);
        //     cout << "Copy Success ID : " << metadata.sequence << endl;
        //     //釋放
        //     munmap(base, offset + length);
        // }
        

        //釋放
        // display only
        // if(metadata.sequence == buffercount - 1)
        {
            unique_lock<mutex> lock(mtx);
            memcpy(DisplayImage.data, unmap.at(buffer).pixels, gWidth*gHeight*3);
            cout << "Copy Success ID : " << metadata.sequence << endl;
            shootOK = true;
            cvar.notify_one();
        }
    }
    // 歸還request給相機
    if (running) {
        request->reuse(Request::ReuseBuffers);
        camera->queueRequest(request);
    }
}

int main()
{
    //初始化記憶體
    // for (auto &m : ImageBuffer) m = cv::Mat::zeros(theight, twidth, CV_8UC3);

    // 偵測相機
    unique_ptr<CameraManager> cm = make_unique<CameraManager>();
    cm->start();

    // 檢查是否有相機
    if(cm->cameras().empty())
    {
       cout << "No cameras were identified on the system." << endl;
       cm->stop();
       return EXIT_FAILURE;
    }
    else 
    {
        cout << "Camera founded successfully." << endl;
    }

    // 鎖定相機給自己用
    camera = cm->cameras().front();
    int acqflag = camera->acquire();
    if(acqflag < 0)
    {
        cout << "Acquired camera fail: " << acqflag << endl;
        camera.reset();
        cm->stop();
        return EXIT_FAILURE;
    }
    cout << "Acquire camera success." << endl;
    
    // set Configuration
    unique_ptr<CameraConfiguration> config = camera->generateConfiguration({StreamRole::Viewfinder});
    config->at(0).size.width = twidth;
    config->at(0).size.height = theight;
    config->at(0).bufferCount = framebuffer;
    config->at(0).pixelFormat = pixelformat;
    config->validate();
    cout << "Stride : " << config->at(0).stride << endl;
    int ret = camera->configure(config.get()); // 相機狀態:Acquired → Configured

    auto sconfig = config->at(0);
    gHeight = sconfig.size.height;
    gWidth = sconfig.size.width;
    DisplayImage = cv::Mat::zeros(gHeight, gWidth, CV_8UC3);

    // Frame allocator
    auto allocator = make_unique<FrameBufferAllocator>(camera);
    for(auto& cfg : *config)
    {
        int ret = allocator->allocate(cfg.stream()); // 請allocator為這條stream配置一組buffer
        if(ret < 0)
        {
            std::cerr << "Can't allocate buffers" << std::endl;
            return -ENOMEM;
        }
        size_t allocated = allocator->buffers(cfg.stream()).size();
        std::cout << "Allocated " << allocated << " buffers for stream" << std::endl;
    }

    // Binding request and framebuffer
    Stream *stream = config->at(0).stream();
    const vector<unique_ptr<FrameBuffer>> &buffers = allocator->buffers(stream); // 從某個stream中拿出buffer
    vector<unique_ptr<Request>> requests; // 建立一個request pool

    for(const auto& buf : allocator->buffers(stream))
    {
        const auto& plane = buf->planes()[0];          // Bayer RAW 只有一個 plane
        void* mem = mmap(nullptr, plane.offset + plane.length,
                        PROT_READ, MAP_SHARED, plane.fd.get(), 0);
        // if (mem == MAP_FAILED) { }
        unmap[buf.get()] = {static_cast<uint8_t*>(mem) + plane.offset, mem, plane.offset + plane.length};
    }

    for(int i = 0; i < buffers.size(); i++)
    {
        unique_ptr<Request> request = camera->createRequest();
        if(!request)
        {
            cerr << "Can't create request" << endl;
            return -ENOMEM;
        }

        const unique_ptr<FrameBuffer> &buffer = buffers[i];
        int ret = request->addBuffer(stream, buffer.get()); // 這裡有綁定buffe，釋放的時候要注意

        if(ret < 0)
        {
            cerr << "Can't set buffer for request" << endl;
            return ret;
        }
        requests.push_back(move(request));
    }

    cout << "Successfully binding request and FrameBuffer" << endl;
    
    // Event Handler
    camera->requestCompleted.connect(requestComplete);
    running = true;
    camera->start();
    for (std::unique_ptr<Request> &request : requests)
        camera->queueRequest(request.get());

    // 暫停主執行續等待相機端做完
    auto lastFrame   = std::chrono::steady_clock::now();   // 上一次顯示的時間
    auto windowStart = std::chrono::steady_clock::now();   // 平均區間的起點
    while(true)
    {
        {
            unique_lock<mutex> lock(mtx);
            cvar.wait(lock, [](){return shootOK;});
            shootOK = false;
            cv::putText(DisplayImage, cv::format("FPS %.1f", fpsAvg), {20, 40},
                    cv::FONT_HERSHEY_SIMPLEX, 1.0, {0, 255, 0}, 2);
            cv::imshow("Live View", DisplayImage);
        }
        
        // 計算FPS
        auto now = std::chrono::steady_clock::now();
        double dt = std::chrono::duration<double>(now - lastFrame).count();   // 單位：秒，有小數
        lastFrame = now;
        double fpsInstant = (dt > 0) ? 1.0 / dt : 0.0;

        ++frameCount;
        double elapsed = std::chrono::duration<double>(now - windowStart).count();
        if (elapsed >= 1.0)
        {
            fpsAvg = frameCount / elapsed;
            cout << "Display FPS: " << fpsAvg << "  (instant " << fpsInstant << ")" << endl;
            frameCount  = 0;
            windowStart = now;
        }

        int key = cv::waitKey(1);
        if(key == 'q' || key == 27) break;
    }
    cv::destroyAllWindows();
    running = false;
    // std::this_thread::sleep_for(3000ms);
    // 停止相機
    camera->stop(); // 如果camera沒有start()，就不用stop()
    camera->requestCompleted.disconnect(requestComplete);

    //這裡要等thread_pool那邊的影像處理完才能往後走
    taskManager.stop();

    for (auto& [buf, m] : unmap)                // 在 free 之前 munmap
        munmap(m.base, m.size);
    unmap.clear();

    requests.clear();
    allocator.reset();
    config.reset();
    camera->release();
    camera.reset();
    cm->stop();
    return 0;
}