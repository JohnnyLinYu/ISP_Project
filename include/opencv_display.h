#pragma once
#include "Display.h"

class cv_display : public Display
{
public:
    void show(const OwnedFrame& of) override;
    bool quit() override;
};