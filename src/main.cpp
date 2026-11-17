#include <iostream>
#include <libcamera/libcamera.h>
#include "../include/CameraControl.h"
#include "../include/Display.h"
#include "../include/opencv_display.h"
#include "../include/LatestFrame.h"
#include "../include/common.h"
using namespace std;

int main()
{
    LatestFrame latest;
    cv_display display;
    CameraTaskManger cam;
    StreamSetting s;                      // 預設 1920×1080、5 塊 buffer、RGB888

    if (!cam.open())                return 1;   // ① 拿到相機
    if (!cam.setConfigure(s))       return 1;   // ② 設定、validate，決定實際寬高
    latest.init(cam.g_width, cam.g_height);     // ③ 用「實際」寬高建立 LatestFrame
    if (!cam.framebufferAllocator()) return 1;  // ④ 配置 buffer
    if (!cam.bindingRequest())      return 1;   // ⑤ mmap + 建立 request
    cam.setFrameCallback([&](const Frame& f) { latest.push(f); });   // ⑥ 接上 LatestFrame
    cam.start();   
    OwnedFrame frame;                         // 迴圈外宣告一次，重複使用
    while (true)
    {
        if (latest.waitAndCopy(frame, std::chrono::milliseconds(100)))
            display.show(frame);

        if (display.quit())                   // 每圈都要呼叫，就算這圈沒有新幀
            break;
    }    
    cam.stop();                 // ① 相機停止 + 等工作執行緒閒下來
    cv::destroyAllWindows();    // ② 關視窗
    return 0;       
}