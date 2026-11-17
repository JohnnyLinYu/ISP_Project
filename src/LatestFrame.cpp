#include "../include/LatestFrame.h"

void LatestFrame::init(unsigned int width, unsigned int height)
{
    display = cv::Mat::zeros(height, width, CV_8UC3);
}
void LatestFrame::push(const Frame& f)
{
    {
        std::unique_lock<std::mutex> lock(mtx);
        memcpy(display.data, f.data, f.stride * f.height);
        _info = f.info;
        _hasNew = true;
        cvar.notify_one();
    }
}

bool LatestFrame::waitAndCopy(OwnedFrame& out, std::chrono::milliseconds timeout)
{
    {
        std::unique_lock<std::mutex> lock(mtx);
        if(!cvar.wait_for(lock, timeout, [this](){return _hasNew;}))
        {
            std::cout << "Timeout..." << std::endl;
            return false;
        }
        display.copyTo(out.image);
        out.info = _info;
        _hasNew = false;
    }
    return true;
}