#pragma warning(disable : 4996) // 解决 stb_image_write.h 文件中的`sprintf`不安全警告

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <atomic>
#include <condition_variable>
#include <fstream>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <random>
#include <thread>
#include <list>

#include "../09_viewer/Timer.hpp"

struct RenderTarget
{
    uint8_t* pixels {nullptr};
    uint32_t width {0};
    uint32_t height {0};
    uint64_t rowPitch {0};
};

static std::string AppName {"Vulkan-Hpp"};
static std::string EngineName {"Vulkan-Hpp"};
static std::vector<const char*> EnableLayerNames {"VK_LAYER_KHRONOS_validation"};
static std::vector<const char*> EnableExtensionNames {VK_EXT_DEBUG_UTILS_EXTENSION_NAME};

constexpr static uint32_t MaxFramesInFlight {3};

struct Vertex
{
    glm::vec2 pos {0.f, 0.f};
    glm::vec3 color {0.f, 0.f, 0.f};
};

struct UniformBufferObject
{
    glm::mat4 model {glm::mat4(1.f)};
    glm::mat4 view {glm::mat4(1.f)};
    glm::mat4 proj {glm::mat4(1.f)};
};

// clang-format off
const std::vector<Vertex> vertices {
    {{-0.5f, -0.5f}, {1.f, 0.f, 0.f}},
    {{-0.5f,  0.5f}, {1.f, 0.f, 0.f}},
    {{ 0.5f,  0.5f}, {0.f, 1.f, 0.f}},
    {{ 0.5f, -0.5f}, {0.f, 1.f, 0.f}},
};

const std::vector<uint16_t> indices {
    0, 1, 2,
    0, 2, 3,
};

// clang-format on

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData
) noexcept
{
    std::clog << "===========================================\n"
              << "Debug::validation layer: " << pCallbackData->pMessage << '\n';

    return VK_FALSE;
}

static std::vector<uint32_t> ReadFile(const std::string& fileName)
{
    std::ifstream file(fileName, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        throw std::runtime_error("failed to open file: " + fileName);
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<uint32_t> buffer(fileSize / (sizeof(uint32_t) / sizeof(char)));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
    file.close();

    return buffer;
}

uint32_t findMemoryType(vk::PhysicalDeviceMemoryProperties const& memoryProperties, uint32_t typeBits, vk::MemoryPropertyFlags requirementsMask)
{
    uint32_t typeIndex = uint32_t(~0);
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
    {
        if ((typeBits & 1) && ((memoryProperties.memoryTypes[i].propertyFlags & requirementsMask) == requirementsMask))
        {
            typeIndex = i;
            break;
        }
        typeBits >>= 1;
    }
    assert(typeIndex != uint32_t(~0));
    return typeIndex;
}

vk::raii::Pipeline makeGraphicsPipeline(
    vk::raii::Device const& m_device,
    vk::raii::PipelineCache const& pipelineCache,
    vk::raii::ShaderModule const& vertexShaderModule,
    vk::SpecializationInfo const* vertexShaderSpecializationInfo,
    vk::raii::ShaderModule const& fragmentShaderModule,
    vk::SpecializationInfo const* fragmentShaderSpecializationInfo,
    uint32_t vertexStride,
    std::vector<std::pair<vk::Format, uint32_t>> const& vertexInputAttributeFormatOffset,
    vk::FrontFace frontFace,
    bool depthBuffered,
    vk::raii::PipelineLayout const& pipelineLayout,
    vk::raii::RenderPass const& renderPass
)
{
    std::array<vk::PipelineShaderStageCreateInfo, 2> pipelineShaderStageCreateInfos = {
        vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eVertex, vertexShaderModule, "main", vertexShaderSpecializationInfo),
        vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eFragment, fragmentShaderModule, "main", fragmentShaderSpecializationInfo)
    };

    std::vector<vk::VertexInputAttributeDescription> vertexInputAttributeDescriptions;
    std::array inputBinding {
        vk::VertexInputBindingDescription {0, sizeof(Vertex), vk::VertexInputRate::eVertex}
    };
    std::array inputAttribute {
        vk::VertexInputAttributeDescription {0, 0, vk::Format::eR32G32Sfloat,    offsetof(Vertex, Vertex::pos)  },
        vk::VertexInputAttributeDescription {1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, Vertex::color)}
    };

    vk::PipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo({}, inputBinding, inputAttribute);

    vk::PipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo(
        vk::PipelineInputAssemblyStateCreateFlags(), vk::PrimitiveTopology::eTriangleList
    );

    vk::PipelineViewportStateCreateInfo pipelineViewportStateCreateInfo(vk::PipelineViewportStateCreateFlags(), 1, nullptr, 1, nullptr);

    vk::PipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo(
        vk::PipelineRasterizationStateCreateFlags(),
        false,
        false,
        vk::PolygonMode::eFill,
        vk::CullModeFlagBits::eNone,
        frontFace,
        false,
        0.0f,
        0.0f,
        0.0f,
        1.0f
    );

    vk::PipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo({}, vk::SampleCountFlagBits::e1);

    vk::PipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo(
        vk::PipelineDepthStencilStateCreateFlags(), depthBuffered, depthBuffered, vk::CompareOp::eLessOrEqual, false, false, {}, {}
    );

    vk::ColorComponentFlags colorComponentFlags(
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
    );
    vk::PipelineColorBlendAttachmentState pipelineColorBlendAttachmentState(
        false,
        vk::BlendFactor::eZero,
        vk::BlendFactor::eZero,
        vk::BlendOp::eAdd,
        vk::BlendFactor::eZero,
        vk::BlendFactor::eZero,
        vk::BlendOp::eAdd,
        colorComponentFlags
    );
    vk::PipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo(
        vk::PipelineColorBlendStateCreateFlags(),
        false,
        vk::LogicOp::eNoOp,
        pipelineColorBlendAttachmentState,
        {
            {1.0f, 1.0f, 1.0f, 1.0f}
    }
    );

    std::array<vk::DynamicState, 2> dynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo(vk::PipelineDynamicStateCreateFlags(), dynamicStates);

    vk::GraphicsPipelineCreateInfo graphicsPipelineCreateInfo(
        vk::PipelineCreateFlags(),
        pipelineShaderStageCreateInfos,
        &pipelineVertexInputStateCreateInfo,
        &pipelineInputAssemblyStateCreateInfo,
        nullptr,
        &pipelineViewportStateCreateInfo,
        &pipelineRasterizationStateCreateInfo,
        &pipelineMultisampleStateCreateInfo,
        &pipelineDepthStencilStateCreateInfo,
        &pipelineColorBlendStateCreateInfo,
        &pipelineDynamicStateCreateInfo,
        pipelineLayout,
        renderPass
    );

    return vk::raii::Pipeline(m_device, pipelineCache, graphicsPipelineCreateInfo);
}

void setImageLayout(
    vk::raii::CommandBuffer const& commandBuffer, vk::Image image, vk::Format format, vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout
)
{
    vk::AccessFlags sourceAccessMask;
    switch (oldImageLayout)
    {
        case vk::ImageLayout::eTransferDstOptimal:
            sourceAccessMask = vk::AccessFlagBits::eTransferWrite;
            break;
        case vk::ImageLayout::ePreinitialized:
            sourceAccessMask = vk::AccessFlagBits::eHostWrite;
            break;
        case vk::ImageLayout::eGeneral: // sourceAccessMask is empty
        case vk::ImageLayout::eUndefined:
            break;
        default:
            assert(false);
            break;
    }

    vk::PipelineStageFlags sourceStage;
    switch (oldImageLayout)
    {
        case vk::ImageLayout::eGeneral:
        case vk::ImageLayout::ePreinitialized:
            sourceStage = vk::PipelineStageFlagBits::eHost;
            break;
        case vk::ImageLayout::eTransferDstOptimal:
            sourceStage = vk::PipelineStageFlagBits::eTransfer;
            break;
        case vk::ImageLayout::eUndefined:
            sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
            break;
        default:
            assert(false);
            break;
    }

    vk::AccessFlags destinationAccessMask;
    switch (newImageLayout)
    {
        case vk::ImageLayout::eColorAttachmentOptimal:
            destinationAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
            break;
        case vk::ImageLayout::eDepthStencilAttachmentOptimal:
            destinationAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
            break;
        case vk::ImageLayout::eGeneral: // empty destinationAccessMask
        case vk::ImageLayout::ePresentSrcKHR:
            break;
        case vk::ImageLayout::eShaderReadOnlyOptimal:
            destinationAccessMask = vk::AccessFlagBits::eShaderRead;
            break;
        case vk::ImageLayout::eTransferSrcOptimal:
            destinationAccessMask = vk::AccessFlagBits::eTransferRead;
            break;
        case vk::ImageLayout::eTransferDstOptimal:
            destinationAccessMask = vk::AccessFlagBits::eTransferWrite;
            break;
        default:
            assert(false);
            break;
    }

    vk::PipelineStageFlags destinationStage;
    switch (newImageLayout)
    {
        case vk::ImageLayout::eColorAttachmentOptimal:
            destinationStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
            break;
        case vk::ImageLayout::eDepthStencilAttachmentOptimal:
            destinationStage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
            break;
        case vk::ImageLayout::eGeneral:
            destinationStage = vk::PipelineStageFlagBits::eHost;
            break;
        case vk::ImageLayout::ePresentSrcKHR:
            destinationStage = vk::PipelineStageFlagBits::eBottomOfPipe;
            break;
        case vk::ImageLayout::eShaderReadOnlyOptimal:
            destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
            break;
        case vk::ImageLayout::eTransferDstOptimal:
        case vk::ImageLayout::eTransferSrcOptimal:
            destinationStage = vk::PipelineStageFlagBits::eTransfer;
            break;
        default:
            assert(false);
            break;
    }

    vk::ImageAspectFlags aspectMask;
    if (newImageLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal)
    {
        aspectMask = vk::ImageAspectFlagBits::eDepth;
        if (format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint)
        {
            aspectMask |= vk::ImageAspectFlagBits::eStencil;
        }
    }
    else
    {
        aspectMask = vk::ImageAspectFlagBits::eColor;
    }

    vk::ImageSubresourceRange imageSubresourceRange(aspectMask, 0, 1, 0, 1);
    vk::ImageMemoryBarrier imageMemoryBarrier(
        sourceAccessMask,
        destinationAccessMask,
        oldImageLayout,
        newImageLayout,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        image,
        imageSubresourceRange
    );
    return commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, nullptr, nullptr, imageMemoryBarrier);
}

vk::raii::DeviceMemory allocateDeviceMemory(
    vk::raii::Device const& m_device,
    vk::PhysicalDeviceMemoryProperties const& memoryProperties,
    vk::MemoryRequirements const& memoryRequirements,
    vk::MemoryPropertyFlags memoryPropertyFlags
)
{
    uint32_t memoryTypeIndex = findMemoryType(memoryProperties, memoryRequirements.memoryTypeBits, memoryPropertyFlags);
    vk::MemoryAllocateInfo memoryAllocateInfo(memoryRequirements.size, memoryTypeIndex);
    return vk::raii::DeviceMemory(m_device, memoryAllocateInfo);
}

template <typename T>
void copyToDevice(vk::raii::DeviceMemory const& deviceMemory, T const* pData, size_t count, vk::DeviceSize stride = sizeof(T))
{
    assert(sizeof(T) <= stride);
    uint8_t* deviceData = static_cast<uint8_t*>(deviceMemory.mapMemory(0, count * stride));
    if (stride == sizeof(T))
    {
        memcpy(deviceData, pData, count * sizeof(T));
    }
    else
    {
        for (size_t i = 0; i < count; i++)
        {
            memcpy(deviceData, &pData[i], sizeof(T));
            deviceData += stride;
        }
    }
    deviceMemory.unmapMemory();
}

template <typename T>
void copyToDevice(vk::raii::DeviceMemory const& deviceMemory, T const& data)
{
    copyToDevice<T>(deviceMemory, &data, 1);
}

template <typename Func>
void oneTimeSubmit(vk::raii::Device const& m_device, vk::raii::CommandPool const& commandPool, vk::raii::Queue const& queue, Func const& func)
{
    vk::raii::CommandBuffer commandBuffer =
        std::move(vk::raii::CommandBuffers(m_device, {*commandPool, vk::CommandBufferLevel::ePrimary, 1}).front());
    commandBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
    func(commandBuffer);
    commandBuffer.end();
    vk::SubmitInfo submitInfo(nullptr, nullptr, *commandBuffer);
    queue.submit(submitInfo, nullptr);
    queue.waitIdle();
}

struct BufferData
{
    BufferData(
        vk::raii::PhysicalDevice const& physicalDevice,
        vk::raii::Device const& m_device,
        vk::DeviceSize size,
        vk::BufferUsageFlags usage,
        vk::MemoryPropertyFlags propertyFlags = vk::MemoryPropertyFlagBits::eDeviceLocal
    )
        : buffer(m_device, vk::BufferCreateInfo({}, size, usage))
        , m_size(size)
        , m_usage(usage)
        , m_propertyFlags(propertyFlags)
    {
        deviceMemory = allocateDeviceMemory(m_device, physicalDevice.getMemoryProperties(), buffer.getMemoryRequirements(), propertyFlags);
        buffer.bindMemory(deviceMemory, 0);
    }

    BufferData(std::nullptr_t)
    {
    }

    /// @brief 将数据拷贝到暂存缓冲
    /// @tparam DataType
    /// @param data
    template <typename DataType>
    void upload(DataType const& data) const
    {
        assert((m_propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent) && (m_propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible));
        assert(sizeof(DataType) <= m_size);

        void* dataPtr = deviceMemory.mapMemory(0, sizeof(DataType));
        memcpy(dataPtr, &data, sizeof(DataType));
        deviceMemory.unmapMemory();
    }

    template <typename DataType>
    void upload(std::vector<DataType> const& data, size_t stride = 0) const
    {
        assert(m_propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible);

        size_t elementSize = stride ? stride : sizeof(DataType);
        assert(sizeof(DataType) <= elementSize);

        copyToDevice(deviceMemory, data.data(), data.size(), elementSize);
    }

    /// @brief 将数据从CPU先拷贝到暂存缓冲，再拷贝到GPU专用缓冲
    /// @tparam DataType
    /// @param physicalDevice
    /// @param m_device
    /// @param commandPool
    /// @param queue
    /// @param data
    /// @param stride
    template <typename DataType>
    void upload(
        vk::raii::PhysicalDevice const& physicalDevice,
        vk::raii::Device const& m_device,
        vk::raii::CommandPool const& commandPool,
        vk::raii::Queue const& queue,
        std::vector<DataType> const& data,
        size_t stride
    ) const
    {
        assert(m_usage & vk::BufferUsageFlagBits::eTransferDst);
        assert(m_propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal);

        size_t elementSize = stride ? stride : sizeof(DataType);
        assert(sizeof(DataType) <= elementSize);

        size_t dataSize = data.size() * elementSize;
        assert(dataSize <= m_size);

        BufferData stagingBuffer(
            physicalDevice,
            m_device,
            dataSize,
            vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
        );
        copyToDevice(stagingBuffer.deviceMemory, data.data(), data.size(), elementSize);

        oneTimeSubmit(m_device, commandPool, queue, [&](vk::raii::CommandBuffer const& commandBuffer) {
            commandBuffer.copyBuffer(*stagingBuffer.buffer, *this->buffer, vk::BufferCopy(0, 0, dataSize));
        });
    }

    vk::raii::DeviceMemory deviceMemory = nullptr;
    vk::raii::Buffer buffer             = nullptr;

private:
    vk::DeviceSize m_size;
    vk::BufferUsageFlags m_usage;
    vk::MemoryPropertyFlags m_propertyFlags;
};

struct ImageData
{
    ImageData(
        vk::raii::PhysicalDevice const& physicalDevice,
        vk::raii::Device const& m_device,
        vk::Format format_,
        vk::Extent2D const& m_extent,
        vk::ImageTiling tiling,
        vk::ImageUsageFlags usage,
        vk::ImageLayout initialLayout,
        vk::MemoryPropertyFlags memoryProperties,
        vk::ImageAspectFlags aspectMask,
        bool createImageView = true
    )
        : format(format_)
        , image(
              m_device,
              {vk::ImageCreateFlags(),
               vk::ImageType::e2D,
               format,
               vk::Extent3D(m_extent, 1),
               1,
               1,
               vk::SampleCountFlagBits::e1,
               tiling,
               usage | vk::ImageUsageFlagBits::eSampled,
               vk::SharingMode::eExclusive,
               {},
               initialLayout}
          )
    {
        deviceMemory = allocateDeviceMemory(m_device, physicalDevice.getMemoryProperties(), image.getMemoryRequirements(), memoryProperties);
        image.bindMemory(deviceMemory, 0);
        imageView = createImageView
            ? vk::raii::ImageView(m_device, vk::ImageViewCreateInfo({}, image, vk::ImageViewType::e2D, format, {}, {aspectMask, 0, 1, 0, 1}))
            : nullptr;
    }

    ImageData(std::nullptr_t)
    {
    }

    // the DeviceMemory should be destroyed before the Image it is bound to; to get that order with the standard destructor
    // of the ImageData, the order of DeviceMemory and Image here matters
    vk::Format format;
    vk::raii::DeviceMemory deviceMemory = nullptr;
    vk::raii::Image image               = nullptr;
    vk::raii::ImageView imageView       = nullptr;
};

class Test
{
    bool m_supportBlit {false};
    vk::Format m_colorFormat {vk::Format::eB8G8R8A8Unorm};
    vk::Extent2D m_extent {800, 600};

    vk::raii::Context m_context {};
    vk::raii::Instance m_instance {nullptr};
    vk::raii::DebugUtilsMessengerEXT m_debugUtilsMessenger {nullptr};
    vk::raii::Device m_device {nullptr};
    vk::raii::Queue m_graphicsAndTransferQueue {nullptr};

    vk::raii::CommandPool m_commandPool {nullptr};
    vk::raii::CommandBuffers m_commandBuffers {nullptr};
    vk::raii::CommandBuffers m_blitImageCommandBuffers {nullptr};

    BufferData m_vertexBufferData {nullptr};
    BufferData m_indexBufferData {nullptr};
    std::vector<BufferData> m_uniformBufferObjects {};

    vk::raii::DescriptorPool m_descriptorPool {nullptr};
    vk::raii::DescriptorSets m_descriptorSets {nullptr};

    vk::raii::RenderPass m_renderPass {nullptr};
    vk::raii::PipelineLayout m_pipelineLayout {nullptr};
    vk::raii::Pipeline m_graphicsPipeline {nullptr};
    std::vector<vk::raii::Framebuffer> m_framebuffers;

    std::vector<ImageData> m_colorImageDatas {};
    std::vector<ImageData> m_saveImageDatas {};

    std::vector<vk::raii::Fence> m_drawFences {};
    std::vector<vk::raii::Fence> m_blitFences {};
    std::vector<vk::raii::Semaphore> m_renderFinishedSemaphores {};

    std::queue<uint32_t, std::list<uint32_t>> m_submitIndices {};
    std::function<void(const RenderTarget&)> m_callback {};

    std::mutex m_mutex {};
    std::condition_variable m_productCV {};
    std::condition_variable m_consumeCV {};

    std::thread m_productThread {};
    std::thread m_consumeThread {};

    bool m_exit {false};
    bool m_consumeExit {false};

    uint32_t m_currentFrameIndex {0};

    std::atomic_uint32_t m_numberOfTasks {0};

public:
    Test()
    {
        vk::DebugUtilsMessageSeverityFlagsEXT severityFlags {
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
        };
        vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags {
            vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance
            | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
        };

        vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoExt {{}, severityFlags, messageTypeFlags, DebugCallback};

        vk::ApplicationInfo applicationInfo {AppName.c_str(), 1, EngineName.c_str(), 1, VK_API_VERSION_1_1};
        vk::InstanceCreateInfo instanceCreateInfo {{}, &applicationInfo, EnableLayerNames, EnableExtensionNames, &debugUtilsMessengerCreateInfoExt};

        m_instance            = vk::raii::Instance {m_context, instanceCreateInfo};
        m_debugUtilsMessenger = vk::raii::DebugUtilsMessengerEXT {
            m_instance,
            {{},
              vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
              vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance
                 | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation,
              &DebugCallback}
        };

        //--------------------------------------------------------------------------------------
        // 选择一个合适的物理设备
        vk::raii::PhysicalDevices physicalDevices(m_instance); // 所有物理设备，继承自 std::vector
        uint32_t usePhysicalDeviceIndex {};
        uint32_t graphicsAndTransferQueueFamilyIndex {};
        for (size_t i = 0; i < physicalDevices.size(); ++i)
        {
            std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevices[i].getQueueFamilyProperties();
            for (size_t j = 0; j < queueFamilyProperties.size(); ++j)
            {
                if ((queueFamilyProperties[j].queueFlags & vk::QueueFlagBits::eGraphics)
                    && (queueFamilyProperties[j].queueFlags & vk::QueueFlagBits::eTransfer))
                {
                    usePhysicalDeviceIndex              = static_cast<uint32_t>(i);
                    graphicsAndTransferQueueFamilyIndex = static_cast<uint32_t>(j);
                }
            }
        }
        auto physicalDevice = physicalDevices[usePhysicalDeviceIndex];

        vk::FormatProperties formatProperties = physicalDevice.getFormatProperties(vk::Format::eB8G8R8A8Unorm);

        if ((formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eBlitSrc)
            && (formatProperties.linearTilingFeatures & vk::FormatFeatureFlagBits::eBlitDst))
        {
            std::cout << "Support blit\n";
            m_supportBlit = true;
        }

        //--------------------------------------------------------------------------------------
        // 创建一个逻辑设备
        float queuePriority = 0.0f;
        vk::DeviceQueueCreateInfo deviceQueueCreateInfo({}, graphicsAndTransferQueueFamilyIndex, 1, &queuePriority);
        vk::DeviceCreateInfo deviceCreateInfo({}, deviceQueueCreateInfo, {}, {}, {});
        m_device = vk::raii::Device(physicalDevice, deviceCreateInfo);

        //--------------------------------------------------------------------------------------
        m_commandPool = vk::raii::CommandPool(m_device, {{vk::CommandPoolCreateFlagBits::eResetCommandBuffer}, graphicsAndTransferQueueFamilyIndex});

        m_commandBuffers          = vk::raii::CommandBuffers(m_device, {m_commandPool, vk::CommandBufferLevel::ePrimary, MaxFramesInFlight});
        m_blitImageCommandBuffers = vk::raii::CommandBuffers(m_device, {m_commandPool, vk::CommandBufferLevel::ePrimary, MaxFramesInFlight});

        m_graphicsAndTransferQueue = vk::raii::Queue(m_device, graphicsAndTransferQueueFamilyIndex, 0);

        m_colorImageDatas.reserve(MaxFramesInFlight);
        m_saveImageDatas.reserve(MaxFramesInFlight);
        for (uint32_t i = 0; i < MaxFramesInFlight; ++i)
        {
            m_colorImageDatas.emplace_back(ImageData(
                physicalDevice,
                m_device,
                m_colorFormat,
                m_extent,
                vk::ImageTiling::eOptimal,
                vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
                vk::ImageLayout::eUndefined,
                vk::MemoryPropertyFlagBits::eDeviceLocal,
                vk::ImageAspectFlagBits::eColor
            ));

            m_saveImageDatas.emplace_back(ImageData(
                physicalDevice,
                m_device,
                m_colorFormat,
                m_extent,
                vk::ImageTiling::eLinear,
                vk::ImageUsageFlagBits::eTransferDst,
                vk::ImageLayout::eUndefined,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                vk::ImageAspectFlagBits::eColor,
                false
            ));
        }

        //--------------------------------------------------------------------------------------
        vk::AttachmentReference colorAttachment(0, vk::ImageLayout::eColorAttachmentOptimal);
        std::array colorAttachments {colorAttachment};
        vk::SubpassDescription subpassDescription(vk::SubpassDescriptionFlags(), vk::PipelineBindPoint::eGraphics, {}, colorAttachments, {}, {}, {});
        std::vector<vk::AttachmentDescription> attachmentDescriptions {
            {{},
             m_colorFormat, vk::SampleCountFlagBits::e1,
             vk::AttachmentLoadOp::eClear,
             vk::AttachmentStoreOp::eStore,
             vk::AttachmentLoadOp::eDontCare,
             vk::AttachmentStoreOp::eDontCare,
             vk::ImageLayout::eUndefined,
             vk::ImageLayout::eTransferSrcOptimal}
        };

        vk::RenderPassCreateInfo renderPassCreateInfo(vk::RenderPassCreateFlags(), attachmentDescriptions, subpassDescription);
        m_renderPass = vk::raii::RenderPass(m_device, renderPassCreateInfo);

        //--------------------------------------------------------------------------------------
        std::vector<uint32_t> vertSPV = ReadFile("../resources/shaders/01_03_base_vert.spv");
        std::vector<uint32_t> fragSPV = ReadFile("../resources/shaders/01_03_base_frag.spv");
        vk::raii::ShaderModule vertexShaderModule(m_device, vk::ShaderModuleCreateInfo(vk::ShaderModuleCreateFlags(), vertSPV));
        vk::raii::ShaderModule fragmentShaderModule(m_device, vk::ShaderModuleCreateInfo(vk::ShaderModuleCreateFlags(), fragSPV));

        std::array descriptorSetLayoutBindings {
            vk::DescriptorSetLayoutBinding {0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex}
        };
        vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo({}, descriptorSetLayoutBindings);
        vk::raii::DescriptorSetLayout descriptorSetLayout(m_device, descriptorSetLayoutCreateInfo);

        std::array<vk::DescriptorSetLayout, 1> descriptorSetLayoursForPipeline {descriptorSetLayout};
        vk::raii::PipelineCache pipelineCache(m_device, vk::PipelineCacheCreateInfo());

        m_pipelineLayout   = vk::raii::PipelineLayout(m_device, {{}, descriptorSetLayoursForPipeline});
        m_graphicsPipeline = makeGraphicsPipeline(
            m_device,
            pipelineCache,
            vertexShaderModule,
            nullptr,
            fragmentShaderModule,
            nullptr,
            0,
            {},
            vk::FrontFace::eClockwise,
            false,
            m_pipelineLayout,
            m_renderPass
        );

        //--------------------------------------------------------------------------------------
        m_vertexBufferData = BufferData(
            physicalDevice, m_device, sizeof(Vertex) * vertices.size(), vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst
        );
        m_vertexBufferData.upload(physicalDevice, m_device, m_commandPool, m_graphicsAndTransferQueue, vertices, sizeof(Vertex));

        m_indexBufferData = BufferData(
            physicalDevice,
            m_device,
            sizeof(std::remove_cvref_t<decltype(indices.front())>) * indices.size(),
            vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst
        );
        m_indexBufferData.upload(
            physicalDevice, m_device, m_commandPool, m_graphicsAndTransferQueue, indices, sizeof(std::remove_cvref_t<decltype(indices.front())>)
        );

        UniformBufferObject defaultUBO {};
        m_uniformBufferObjects.reserve(MaxFramesInFlight);
        for (uint32_t i = 0; i < MaxFramesInFlight; ++i)
        {
            m_uniformBufferObjects.emplace_back(BufferData(
                physicalDevice,
                m_device,
                sizeof(UniformBufferObject),
                vk::BufferUsageFlagBits::eUniformBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
            ));

            copyToDevice(m_uniformBufferObjects[i].deviceMemory, defaultUBO);
        }

        //--------------------------------------------------------------------------------------
        std::array descriptorPoolSizes {
            vk::DescriptorPoolSize {vk::DescriptorType::eUniformBuffer, MaxFramesInFlight}
        };

        m_descriptorPool =
            vk::raii::DescriptorPool(m_device, {vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, MaxFramesInFlight, descriptorPoolSizes});

        std::vector<vk::DescriptorSetLayout> descriptorSetLayouts;
        descriptorSetLayouts.reserve(MaxFramesInFlight);
        for (uint32_t i = 0; i < MaxFramesInFlight; ++i)
        {
            descriptorSetLayouts.emplace_back(descriptorSetLayout);
        }

        m_descriptorSets = vk::raii::DescriptorSets(m_device, {m_descriptorPool, descriptorSetLayouts});

        for (uint32_t i = 0; i < MaxFramesInFlight; ++i)
        {
            vk::DescriptorBufferInfo descriptorBufferInfo(m_uniformBufferObjects[i].buffer, 0, sizeof(UniformBufferObject));
            std::array writeDescriptorSets {
                vk::WriteDescriptorSet {m_descriptorSets[i], 0, 0, vk::DescriptorType::eUniformBuffer, nullptr, descriptorBufferInfo}
            };
            m_device.updateDescriptorSets(writeDescriptorSets, nullptr);
        }

        //--------------------------------------------------------------------------------------
        m_framebuffers.reserve(MaxFramesInFlight);
        for (uint32_t i = 0; i < MaxFramesInFlight; i++)
        {
            std::array<vk::ImageView, 1> imageViews {m_colorImageDatas[i].imageView};
            m_framebuffers.emplace_back(
                vk::raii::Framebuffer(m_device, vk::FramebufferCreateInfo({}, m_renderPass, imageViews, m_extent.width, m_extent.height, 1))
            );
        }

        //--------------------------------------------------------------------------------------
        m_drawFences.reserve(MaxFramesInFlight);
        m_blitFences.reserve(MaxFramesInFlight);
        m_renderFinishedSemaphores.reserve(MaxFramesInFlight);
        for (uint32_t i = 0; i < MaxFramesInFlight; i++)
        {
            m_drawFences.emplace_back(vk::raii::Fence(m_device, vk::FenceCreateInfo()));
            m_blitFences.emplace_back(vk::raii::Fence(m_device, vk::FenceCreateInfo()));
            m_renderFinishedSemaphores.emplace_back(vk::raii::Semaphore(m_device, vk::SemaphoreCreateInfo()));
        }

        Run();
    }

    ~Test()
    {
        m_productThread.join();
        m_consumeThread.join();

        m_device.waitIdle();
    }

    void Run()
    {
        m_productThread = std::thread([this]() { this->Product(); });
        m_consumeThread = std::thread([this]() { this->Consume(); });
    }

    void AddTask()
    {
        m_numberOfTasks++;
        m_productCV.notify_all();
    }

    void SetRTCallback(std::function<void(const RenderTarget&)>&& callback)
    {
        m_callback = std::move(callback);
    }

    void Exit()
    {
        m_exit = true;
    }

    void Product()
    {
        while (!(m_exit && m_numberOfTasks == 0))
        {
            static uint32_t index {0};
            {
                std::unique_lock lk(m_mutex);
                if (!m_productCV.wait_for(lk, std::chrono::seconds(9), [this]() {
                        return !(this->m_numberOfTasks == 0 || this->m_submitIndices.size() == MaxFramesInFlight);
                    }))
                {
                    continue;
                }
            }

            UniformBufferObject defaultUBO {};
            defaultUBO.model = glm::rotate(glm::mat4(1.f), glm::radians(30.f * index), glm::vec3(0.f, 0.f, 1.f));
            copyToDevice(m_uniformBufferObjects[m_currentFrameIndex].deviceMemory, defaultUBO);

            //--------------------------------------------------------------------------------------
            auto&& cmd = m_commandBuffers[m_currentFrameIndex];
            cmd.reset();
            cmd.begin({});

            std::array<vk::ClearValue, 1> clearValues;
            clearValues[0].color = vk::ClearColorValue(0.0f + 1.f / index, 0.2f, 0.3f, 1.f);
            vk::RenderPassBeginInfo renderPassBeginInfo(
                m_renderPass, m_framebuffers[m_currentFrameIndex], vk::Rect2D(vk::Offset2D(0, 0), m_extent), clearValues
            );

            cmd.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);

            cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphicsPipeline);
            cmd.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(m_extent.width), static_cast<float>(m_extent.height), 0.0f, 1.0f));
            cmd.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), m_extent));

            cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout, 0, {m_descriptorSets[m_currentFrameIndex]}, nullptr);
            cmd.bindVertexBuffers(0, {m_vertexBufferData.buffer}, {0});
            cmd.bindIndexBuffer(m_indexBufferData.buffer, 0, vk::IndexType::eUint16);
            cmd.drawIndexed(static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

            cmd.endRenderPass();

            cmd.end();

            vk::CommandBuffer pDrawCommandBuffer[] {cmd};
            vk::Semaphore semaphore[] {m_renderFinishedSemaphores[m_currentFrameIndex]};
            vk::SubmitInfo drawSubmitInfo(0, nullptr, nullptr, 1, pDrawCommandBuffer, 1, semaphore);
            m_graphicsAndTransferQueue.submit(drawSubmitInfo, *m_drawFences[m_currentFrameIndex]);

            //--------------------------------------------------------------------------------------
            auto&& blitCmd = m_blitImageCommandBuffers[m_currentFrameIndex];
            blitCmd.reset();
            blitCmd.begin({});

            setImageLayout(
                blitCmd, m_saveImageDatas[m_currentFrameIndex].image, m_colorFormat, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal
            );

            if (m_supportBlit)
            {
                vk::ImageSubresourceLayers imageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1);
                std::array<vk::Offset3D, 2> offsets {
                    vk::Offset3D(0, 0, 0), vk::Offset3D {static_cast<int32_t>(m_extent.width), static_cast<int32_t>(m_extent.height), 1}
                };
                vk::ImageBlit imageBlit(imageSubresourceLayers, offsets, imageSubresourceLayers, offsets);
                blitCmd.blitImage(
                    m_colorImageDatas[m_currentFrameIndex].image,
                    vk::ImageLayout::eTransferSrcOptimal,
                    m_saveImageDatas[m_currentFrameIndex].image,
                    vk::ImageLayout::eTransferDstOptimal,
                    imageBlit,
                    vk::Filter::eLinear
                );
            }
            else
            {
                vk::ImageSubresourceLayers imageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1);
                vk::ImageCopy imageCopy(imageSubresourceLayers, vk::Offset3D(), imageSubresourceLayers, vk::Offset3D(), vk::Extent3D {m_extent, 1});
                blitCmd.copyImage(
                    m_colorImageDatas[m_currentFrameIndex].image,
                    vk::ImageLayout::eTransferSrcOptimal,
                    m_saveImageDatas[m_currentFrameIndex].image,
                    vk::ImageLayout::eTransferDstOptimal,
                    imageCopy
                );
            }

            setImageLayout(
                blitCmd, m_saveImageDatas[m_currentFrameIndex].image, m_colorFormat, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eGeneral
            );

            blitCmd.end();

            vk::CommandBuffer pBlitCommandBuffer[] {blitCmd};
            vk::PipelineStageFlags waitStages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
            vk::SubmitInfo blitSubmitInfo(1, semaphore, waitStages, 1, pBlitCommandBuffer);
            m_graphicsAndTransferQueue.submit(blitSubmitInfo, m_blitFences[m_currentFrameIndex]);

            {
                std::lock_guard lk(m_mutex);
                m_submitIndices.emplace(m_currentFrameIndex);
            }

            m_numberOfTasks--;
            m_currentFrameIndex = (m_currentFrameIndex + 1) % MaxFramesInFlight;
            m_consumeCV.notify_all();
        }

        m_consumeExit = true;
    }

    void Consume()
    {
        while (!(m_consumeExit && m_submitIndices.empty()))
        {
            uint32_t frameIndex {0};
            {
                std::unique_lock lk(m_mutex);
                if (!m_consumeCV.wait_for(lk, std::chrono::seconds(9), [this]() { return !this->m_submitIndices.empty(); }))
                {
                    continue;
                }
                frameIndex = m_submitIndices.front();
            }

            auto result = m_device.waitForFences({m_drawFences[frameIndex], m_blitFences[frameIndex]}, VK_TRUE, std::numeric_limits<uint64_t>::max());

            m_device.resetFences({m_drawFences[frameIndex], m_blitFences[frameIndex]});

            auto subResourceLayout = m_saveImageDatas[frameIndex].image.getSubresourceLayout({vk::ImageAspectFlagBits::eColor, 0, 0});

            auto saveImagePixels = reinterpret_cast<uint8_t*>(m_saveImageDatas[frameIndex].deviceMemory.mapMemory(0, vk::WholeSize, {}));
            saveImagePixels += subResourceLayout.offset;

            m_callback({saveImagePixels, m_extent.width, m_extent.height, subResourceLayout.rowPitch});

            m_saveImageDatas[frameIndex].deviceMemory.unmapMemory();

            {
                std::lock_guard lk(m_mutex);
                m_submitIndices.pop();
            }

            m_productCV.notify_all();
        }
    }
};

int main()
{
    try
    {
        std::cout << "Main thread id: " << std::this_thread::get_id() << '\n';

        Timer time("run");

        Test test {};
        test.SetRTCallback([](const RenderTarget& rt) {
            static int count {0};
            Timer time2("save");
            std::cout << "save: " << count << std::endl;
            stbi_write_jpg(("raii_" + std::to_string(count++) + ".jpg").c_str(), rt.width, rt.height, 4, rt.pixels, 100);
        });

        for (auto i = 0u; i < 10u; ++i)
        {
            test.AddTask();
        }

        test.Exit();
    }
    catch (vk::SystemError& err)
    {
        std::cout << "vk::SystemError: " << err.what() << std::endl;
        exit(-1);
    }
    catch (std::exception& err)
    {
        std::cout << "std::exception: " << err.what() << std::endl;
        exit(-1);
    }
    catch (...)
    {
        std::cout << "unknown error\n";
        exit(-1);
    }

    std::cout << "Success\n";
    return 0;
}
