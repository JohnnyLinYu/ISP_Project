#pragma once
#include "common.h"
#include <opencv2/core/hal/interface.h>
#include <opencv2/calib3d.hpp>
#include <opencv2/opencv.hpp>
class Display 
{
public:
    virtual ~Display() = default;
    virtual void show(const OwnedFrame& of) = 0;
    virtual bool quit() = 0;
};