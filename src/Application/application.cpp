#include "GLFW/glfw3.h"

#include "application.h"

#include <stdexcept>

FApplication::FApplication()
{
    if (!glfwInit())
        throw std::runtime_error("GLFW initialization failed");
}

FApplication::~FApplication()
{
    glfwTerminate();
}

int FApplication::Run()
{
    Window = glfwCreateWindow(1920, 1020, "Game of Life", nullptr, nullptr);
    glfwMakeContextCurrent(Window);

    while (!glfwWindowShouldClose(Window))
    {
        glfwSwapBuffers(Window);
        glfwPollEvents();
    }

    return 0;
}
