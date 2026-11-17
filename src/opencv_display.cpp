#include "../include/opencv_display.h"
    
void cv_display::show(const OwnedFrame& of)
{
    cv::imshow("Live View", of.image);
}
bool cv_display::quit()
{
    int key = cv::waitKey(1);
    return key == 'q' || key == 27;
}