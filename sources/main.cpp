
/*
 * Vulkan 画一个三角形流程：
 * 01.创建一个 VkInstance
 * 02.选择一个支持 Vulkan 的图形设备 (VkPhysicalDevice)
 * 03.为绘制和显示操作创建 VkDevice 和 VkQueue
 * 04.创建一个窗口，窗口表面和交换链
 * 05.将交换链图像包装进 VkImageView
 * 06.创建一个渲染层指定渲染目标和使用方式
 * 07.为渲染层创建帧缓冲
 * 08.配置图形管线
 * 09.为每一个交换链图像分配指令缓冲
 * 10.从交换链获取图像进行绘制操作，提交图像对应的指令缓冲，返回图像到交换链
 */

#define GLFW_INCLUDE_VULKAN // 定义这个宏之后，glfw3.h文件就会包含vulkan的头文件
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>

class HelloTriangleApplication
{
private:
    GLFWwindow* m_window { nullptr };

public:
    void Run()
    {
        InitWindow();
        InitVulkan();
        MainLoop();
        Cleanup();
    }

private:
    void InitWindow()
    {
        if (GLFW_FALSE == glfwInit())
        {
            throw std::runtime_error("GLFW init failed");
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        m_window = glfwCreateWindow(800, 600, "Vulkan", nullptr, nullptr);
    }

    void InitVulkan()
    {
    }

    void MainLoop()
    {
        while (!glfwWindowShouldClose(m_window))
        {
            glfwPollEvents();
        }
    }

    void Cleanup()
    {
        glfwDestroyWindow(m_window);
        glfwTerminate();
    }
};

int main()
{
    HelloTriangleApplication app;

    try
    {
        app.Run();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
