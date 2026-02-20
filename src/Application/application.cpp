#include "GLFW/glfw3.h"
#define GLFW_EXPOSE_NATIVE_WIN32
#include "GLFW/glfw3native.h"
#include "dx12_wrappers.h"

#include "application.h"

#include <stdexcept>

FApplication::FApplication() : Width(1920), Height(1080)
{
    if (Width % 32 != 0)
        throw std::invalid_argument("Width must be a multiple of 32");
    if (!glfwInit())
        throw std::runtime_error("GLFW initialization failed");

    Window = glfwCreateWindow(Width, Height, "Game of Life", nullptr, nullptr);
    glfwMakeContextCurrent(Window);

    HWND WindowHandle = glfwGetWin32Window(Window);
    DX12Context.Initialize(WindowHandle, Width, Height);
}

FApplication::~FApplication()
{
    DX12Context.Shutdown();
    glfwTerminate();
}

int FApplication::Run()
{
    while (!glfwWindowShouldClose(Window))
    {
        DX12Context.Dispatch(Width / 8, Height / 8, 1);
        DX12Context.Present();
        glfwPollEvents();
    }

    return 0;
}
