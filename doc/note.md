## Vulkan绘制图像的步骤
以下步骤以 01_06_loadingModels 为基础
### 1.初始化GLFW并创建GLFW窗口
### 2.创建Vulkan实例用来初始化Vulkan，
- 启用全局扩展（是相对整个程序而言的，而不是某个GPU）：窗口系统扩展、调试报告扩展等
- 开启校验层
### 3.设置回调函数用来接收调试信息
### 4.创建表面Surface
- 为了在不同平台通用，可以使用GLFW创建
- Vulkan实例创建完之后选择物理设备之前必须立即创建表面，因为它会影响物理设备（GPU）的选择
### 5.选择物理设备
- 检查物理设备是否满足要求，例如：纹理的压缩、64位浮点数、多视图渲染等等
- 查找满足需求的队列簇，不同的队列簇支持不同类型的指令，例如：计算、内存传输、绘图、呈现等
- 检查扩展是否支持，例如交换链扩展：表面支持的格式、表面支持的呈现模式
### 6.创建逻辑设备
- 逻辑设备是和物理设备交互的接口
- Vulkan实例开启了校验层，此处根据需要也开启相同的校验层
- 获取指定队列簇（图形队列、呈现队列）的队列句柄，指令需要提交到队列
### 7.创建交换链
- 交换链本质上是一个图像队列，Vulkan在这个图像队列的每一个图像上绘制内容，然后将交换链中的图像显示到屏幕
- 查询交换链支持的细节
- 根据交换链细节选择合适的表面模式
- 根据交换链细节选择合适的呈现模式，代表了屏幕呈现图像的条件
- 设置交换范围，即图像的分辨率
- 指定在图像上进行的操作，例如：显示（颜色附件）、后处理等
- 获取交换链图像
### 8.创建图像视图
- 任何VkImage对象都需要通过VkImageView来绑定访问，包括处于交换链中的、处于渲染管线中的
- 交换链有几张图像就创建几个图像视图
### 9.创建渲染流程对象
- 用于渲染的帧缓冲附着，指定颜色和深度缓冲
- 设置片段着色器中的输出`layout(location = N)out`
### 10.创建着色器使用的每一个描述符集布局
- 类似OpenGL的Uniform
- 描述符池必须包含描述符绑定信息才可以分配对应的描述符集
- 和Shader中的`layout(binding = N)`关联起来
### 11.创建图形管线
完整的图形管线包括：着色器阶段、固定功能状态、管线布局、渲染流程
在Vulkan中几乎不允许对图形管线进行动态设置，也就意味着每一种状态都需要提前创建一个图形管线（填充模式、图元类型等等）
- 创建Shader（顶点、片段...），设置着色器常量、设置着色器入口函数
- 顶点信息布局（pos,color）
- 拓扑信息，指定图元类型：点、线、三角形
- 视口
- 裁剪
- 深度值范围
- 光栅化：表面剔除、正面背面设置、填充或线框
- 多重采样
- 深度、模板测试、颜色混合（每个帧缓冲可以单独设置混合）
- 动态状态（可以允许动态设置而不用重新创建图形管线）：线宽、视口大小、混合常量
- 管线布局：描述符绑定信息（Shader中使用的uniform）、push_constant等
### 12.创建指令池
- 用于管理指令缓冲对象使用的内存并负责指令缓冲对象的分配，创建指令池需要指定队列簇，指令提交时需要和创建时的队列簇保持一致。
### 13.配置深度缓冲需要的资源
- 创建一个图像（对应的图像视图也需要创建）用来缓存深度信息
### 14.创建帧缓冲对象
- 附着（颜色、深度、模板等）需要绑定到帧缓冲对象上使用
- 交换链有几张图像，就需要创建几个帧缓冲对象，交换链中的图像就是帧缓冲的颜色附着
### 15.加载纹理
- 创建Buffer将纹理像素数据映射到Buffer（暂存Buffer）
- 创建图像、分配图像内存，将Buffer（暂存Buffer）中的数据拷贝到图像
- vkCreateBuffer vkAllocateMemory vkMapMemory std::memcpy vkUnmapMemory vkCreateImage vkAllocateMemory vkCmdCopyBufferToImage
### 16.创建纹理采样器
- 指定纹理缩放时的插值方式
- 各向异性过滤（需要检查物理设备是否支持）
- 设置细化级别
- 边界颜色
### 17.创建顶点缓冲
- 使用一个临时缓冲（暂存），先将顶点数据加载到临时缓冲再复制到顶点缓冲
- vkCreateBuffer vkAllocateMemory vkMapMemory std::memcpy vkUnmapMemory vkCmdCopyBuffer
### 18.创建索引缓冲
- 类似顶点缓冲
### 19.创建Uniform Buffer
- Uniform Buffer需要频繁更新，所以此处使用暂存缓冲并不会带来性能提升
- vkCreateBuffer vkAllocateMemory
- 每一帧（GPU并行处理的帧，不是交换链的图像）都需要创建一份Uniform Buffer
### 20.创建描述符池
- 描述符池可以用来创建描述符集
### 21.创建描述符集
- 每一帧都需要一个描述符集
- 用来描述Shader中的Uniform的信息
### 22.创建指令缓冲对象
- 数量需要和交换链中的图像个数一致
- 用来记录绘制指令
### 23.创建同步对象
- 用来发送信号：图像被获取后就可以渲染、图像渲染完成后就可以呈现
- 同步对象的数量和并行帧的个数一致
### 24.渲染循环
- 等待上一次提交的指令结束执行就可以开始绘制新的一帧
- 从交换链获取一张图像
- 更新Uniform
- 提交指令缓冲给图形指令队列 vkQueueSubmit
- 使用交换链进行图像呈现 vkQueuePresentKHR
```c++
UpdateUniform()

vkBeginCommandBuffer()
  vkCmdBeginRenderPass()
    vkCmdBindPipeline()
    vkCmdSetViewport()
    vkCmdSetScissor()
    vkCmdBindVertexBuffers()
    vkCmdBindIndexBuffer()
    vkCmdBindDescriptorSets()
    vkCmdDrawIndexed()
  vkCmdEndRenderPass()
vkEndCommandBuffer()
```
### 25.重建交换链
当窗口大小改变时，交换链中的图像也需要跟着改变
- 根据新的窗口大小创建新的交换链 vkCreateSwapchainKHR
- 创建交换链中图像的视图 vkCreateImageView
- 创建新的深度图像资源：深度图像以及深度图像视图 vkCreateImage vkCreateImageView
- 创建新的帧缓冲，交换链图像和深度图像改变，相当于帧缓冲的附件改变了，帧缓冲也需要重新创建 vkCreateFramebuffer

### 描述符集、描述符池、描述符绑定信息、UniformBuffer
- 以下三个值(N)共同指定了描述符集的绑定点
  - VkWriteDescriptorSet.dstBinding = N;
  - VkDescriptorSetLayoutBinding.binding = N;
  - 着色器中的 layout(binding = N) uniform name{vec4 transform;};
- 描述符集布局
  - vkCreateDescriptorSetLayout 创建描述符集布局，指定着色器的哪个阶段都有那些Uniform，比如顶点着色器阶段有MVP矩阵，片段着色器阶段有采样器，还指定了着色器的绑定点
  - VkDescriptorSetLayoutBinding 指定类型：UniformBuffer还是Sampler，绑定点（和Shader对应），着色器阶段（顶点、片段）
  - VkPipelineLayoutCreateInfo 创建管线布局时会用到描述符集布局
- 描述符池
  - vkCreateDescriptorPool 指定可以创建X个Y类型的描述符集，比如创建2个UniformBuffer类型的描述符集，或者创建1个Sampler类型的描述符集
  - VkDescriptorPoolSize 指定对应的描述符池可以分配的各种类型的描述符数量
  - VkDescriptorPoolCreateInfo.maxSets 指定最大可分配的描述符集（不是描述符）数量
- 描述符集
  - vkAllocateDescriptorSets 创建描述符集，将描述符集布局和描述符集关联起来
  - vkUpdateDescriptorSets 将描述符集和UniformBuffer或采样器关联起来 
  - VkDescriptorBufferInfo 指定具体哪一个UniformBuffer(VkBuffer)，以及Buffer大小
  - VkDescriptorImageInfo  指定采样器（VkSampler），图像视图（VkImageView）
- 描述符
  - VkWriteDescriptorSet 指定绑定点（和Shader对应），类型（UniformBuffer、Sampler），数据（VkDescriptorBufferInfo VkDescriptorImageInfo）

描述符可以简单理解为GLSL中的`uniform sampler`等。描述符集可以简单理解为一整个渲染（计算）管线使用的所有`uniform sampler`等，每使用一次管线，需要使用`vkCmdBindDescriptorSets`绑定一次描述符集。
### Buffer和Image
- 整体步骤：
  - 1. 创建暂存Buffer，申请内存，绑定Buffer和内存
  - 2. 映射内存到CPU，填充内存
  - 3. 创建真正的Buffer，申请内存，绑定Buffer和内存
  - 4. 拷贝暂存Buffer到真正的Buffer
  - 5. 销毁暂存Buffer、释放暂存Buffer对应的内存
- VertexBuffer
  - vkCreateBuffer          创建VkBuffer，是一个暂存VkBuffer（CPU可见）
  - vkAllocateMemory        申请内存VkDeviceMemory
  - vkBindBufferMemory      将VkBuffer和VkDeviceMemory绑定
  - vkMapMemory             将VkDeviceMemory和主机内存(void*)建立映射关系
  - std::memcpy             拷贝实际的顶点数据
  - vkUnmapMemory           结束内存映射
  - vkCreateBuffer          创建真正的顶点缓冲（显卡读取更快）
  - vkAllocateMemory        申请内存
  - vkBindBufferMemory      将缓冲对象和内存绑定
  - vkCmdCopyBuffer         将暂存缓冲中的数据拷贝到真正的缓冲
  - vkDestroyBuffer         释放暂存缓冲VkBuffer
  - vkFreeMemory            释放暂存缓冲对应的内存
- IndexBuffer
  和顶点缓冲步骤完全一致
- UniformBuffer使用步骤
  - vkCreateBuffer          创建VkBuffer，因为UniformBuffer需要频繁更新所以没必要使用暂存缓冲
  - vkAllocateMemory        申请内存VkDeviceMemory
  - vkBindBufferMemory      将VkBuffer和VkDeviceMemory绑定
  - vkMapMemory             将VkDeviceMemory和主机内存(void*)建立映射关系
- 纹理使用步骤
  - vkCreateBuffer          创建VkBuffer，这里只是一个暂存VkBuffer，需要使用图像数据填充它，这样效率会高很多
  - vkAllocateMemory        申请内存VkDeviceMemory
  - vkBindBufferMemory      将VkBuffer和VkDeviceMemory绑定
  - vkMapMemory             将VkDeviceMemory和主机内存(void*)建立映射关系
  - std::memcpy             拷贝实际的像素数据
  - vkUnmapMemory
  - vkCreateImage           创建图像
  - vkAllocateMemory
  - vkBindImageMemory
  - vkCmdPipelineBarrier    图像布局变换
  - vkCmdCopyBufferToImage  将VkBuffer中的暂存数据拷贝到VkImage，就是将纹理的像素数据拷贝到VkImage
  - vkCmdPipelineBarrier    图像布局变换
  - vkDestroyBuffer         释放暂存缓冲VkBuffer
  - vkFreeMemory            释放暂存缓冲对应的内存
  - vkCreateImageView       创建图像视图，图像只能通过图像视图访问
  - vkCreateSampler         创建采样器

### 图形管线、帧缓冲、附件
- 图形管线：
  - 着色器 VkShaderModule
  - 顶点信息 VkPipelineVertexInputStateCreateInfo 顶点的布局以及顶点着色器的绑定点
  - 拓扑信息 VkPipelineInputAssemblyStateCreateInfo 图元类型：点、线、三角形
  - 视口（裁剪）VkPipelineViewportStateCreateInfo
  - 光栅化（正反面、面剔除、填充模式）VkPipelineRasterizationStateCreateInfo
  - 多重采样 VkPipelineMultisampleStateCreateInfo
  - 深度模板测试 VkPipelineDepthStencilStateCreateInfo 比较方式、是否只读等
  - 颜色混合 VkPipelineColorBlendAttachmentState 
  - 动态状态（视口大小、线宽、颜色混合常量等等）VkPipelineDynamicStateCreateInfo
  - 管线布局（描述符集布局、着色器常量push_constant）VkPipelineLayoutCreateInfo
  - 渲染流程 VkRenderPass
- 渲染流程（RenderPass）
  - 附件描述 VkAttachmentDescription 附件的布局、操作方式、格式等，并不指定具体的附件
  - 子流程 VkSubpassDescription 设置子流程引用的附件，在这里绑定片段着色器的输出：layout(location = N) out vec4 fragColor;
  - 子流程依赖的关系 VkSubpassDependency
- 帧缓冲
  - 将颜色附件、深度附件等设置给帧缓冲，附件就是图像视图

### 渲染循环
以下命令都是异步执行，因此每次渲染循环开始时都应该判断上一次提交的指令是否执行结束，否则GPU需要处理的指令会大量堆积
- vkAcquireNextImageKHR “获取到图像”后发送一个信号
- vkQueueSubmit 收到“获取图像成功的信号”后开始执行绘制命令，绘制完成后发送“绘制完成的信号”
- vkQueuePresentKHR 收到“绘制完成”的信号后将该图像呈现到屏幕

录制绘制指令
- 开始记录指令          vkBeginCommandBuffer()
- 开始一个渲染流程      vkCmdBeginRenderPass()
- 绑定图形管线          vkCmdBindPipeline()
- 设置视口              vkCmdSetViewport()
- 设置裁剪              vkCmdSetScissor()
- 绑定顶点缓冲          vkCmdBindVertexBuffers()
- 绑定索引缓冲          vkCmdBindIndexBuffer()
- 绑定描述符集          vkCmdBindDescriptorSets()
- 执行绘制              vkCmdDrawIndexed()
- 结束这次渲染流程       vkCmdEndRenderPass()
- 结束记录指令          vkEndCommandBuffer()
