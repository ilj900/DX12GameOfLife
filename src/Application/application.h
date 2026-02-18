#pragma once

class GLFWwindow;

class FApplication
{
public:
    FApplication();
    ~FApplication();
    int Run();

private:
    GLFWwindow* Window = nullptr;
};
