#pragma once

#include "dx12_wrappers.h"

#include <memory>

class GLFWwindow;

class FApplication
{
public:
    FApplication();
    ~FApplication();
    int Run();

private:
    GLFWwindow* Window = nullptr;
    FDX12Context DX12Context;
    uint32_t Width = 0;
    uint32_t Height = 0;
};
