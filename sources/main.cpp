
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

#define GLFW_INCLUDE_VULKAN // 定义这个宏之后 glfw3.h 文件就会包含 Vulkan 的头文件
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <vector>

class HelloTriangleApplication
{
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
            throw std::runtime_error("failed to init GLFW");
        }

        // 禁止 glfw 创建 OpenGL 上下文
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        // 禁止窗口大小的改变
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        m_window = glfwCreateWindow(800, 600, "Vulkan", nullptr, nullptr);
    }

    void InitVulkan()
    {
        CreateInstance();
    }

    void MainLoop() const noexcept
    {
        while (!glfwWindowShouldClose(m_window))
        {
            glfwPollEvents();
        }
    }

    void Cleanup() noexcept
    {
        // 程序结束前清理 Vulkan 实例
        vkDestroyInstance(m_instance, nullptr);

        glfwDestroyWindow(m_window);
        glfwTerminate();
    }

private:
    void CreateInstance()
    {
        // 应用程序的信息，这些信息可能会作为驱动程序的优化依据，让驱动做一些特殊的优化
        VkApplicationInfo appInfo  = {};
        appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pNext              = nullptr;
        appInfo.pApplicationName   = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName        = "No Engine";
        appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion         = VK_API_VERSION_1_0;

        // Vulkan 是一个与平台无关的 API ，所以需要一个和窗口系统交互的扩展
        // 使用 glfw 获取这个扩展
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        std::cout << "-------------------------------------------\n"
                  << "GLFW extensions:\n";
        for (size_t i = 0; i < glfwExtensionCount; ++i)
        {
            std::cout << glfwExtensions[i] << '\n';
        }

        // 指定驱动程序需要使用的全局扩展和校验层，全局是指对整个应用程序都有效，而不仅仅是某一个设备
        VkInstanceCreateInfo createInfo = {};
        createInfo.sType                = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo     = &appInfo;

        // 指定需要的全局扩展
        createInfo.enabledExtensionCount   = glfwExtensionCount;
        createInfo.ppEnabledExtensionNames = glfwExtensions;

        // 指定全局校验层
        createInfo.enabledLayerCount = 0;

        // 获取 Vulkan 支持的所有扩展
        uint32_t extensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> extensions(extensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

        std::cout << "-------------------------------------------\n"
                  << "All supported extensions:\n";
        for (const auto& e : extensions)
        {
            std::cout << e.extensionName << '\n';
        }

        // 创建 Vulkan 实例，用来初始化 Vulkan 库
        // 1.包含创建信息的结构体指针
        // 2.自定义的分配器回调函数
        // 3.指向实例句柄存储位置的指针
        if (VK_SUCCESS != vkCreateInstance(&createInfo, nullptr, &m_instance))
        {
            throw std::runtime_error("failed to create instance");
        }
    }

private:
    GLFWwindow* m_window { nullptr };
    VkInstance m_instance { nullptr };
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
