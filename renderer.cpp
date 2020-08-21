#include "renderer.h"

#include "vk_init.h"
#include "vk_utils.h"

Renderer::Renderer(VulkanWindow* window)
  : VulkanBase(window)
{
  createResources();
}

std::string
LoadFile(const char* _filename)
{
  std::string buff;

  FILE* file = 0;
  fopen_s(&file, _filename, "rb");
  if (file) {
    fseek(file, 0, SEEK_END);
    size_t bytes = ftell(file);

    buff.resize(bytes);

    fseek(file, 0, SEEK_SET);
    fread(&(*buff.begin()), 1, bytes, file);
    fclose(file);
    return buff;
  }

  return buff;
}

VkShaderModule
LoadShaderModule(VkDevice device, const char* filename)
{
  std::string buff = LoadFile(filename);
  auto result =
    vkuCreateShaderModule(device, buff.size(), (uint32_t*)buff.data(), nullptr);
  ASSERT_VK_VALID_HANDLE(result);
  return result;
}

void
Renderer::createResources()
{
  // depth stencil image / view
  auto depthStencilFormat = VK_FORMAT_D24_UNORM_S8_UINT;

  VkImageCreateInfo depthStencilImageInfo = vkiImageCreateInfo(
    VK_IMAGE_TYPE_2D,
    depthStencilFormat,
    { swapchain->imageExtent.width, swapchain->imageExtent.height, 1 },
    1,
    1,
    VK_SAMPLE_COUNT_1_BIT,
    VK_IMAGE_TILING_OPTIMAL,
    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
    VK_SHARING_MODE_EXCLUSIVE,
    VK_QUEUE_FAMILY_IGNORED,
    nullptr,
    VK_IMAGE_LAYOUT_UNDEFINED);

  ASSERT_VK_SUCCESS(
    vkCreateImage(device, &depthStencilImageInfo, nullptr, &depthStencilImage));

  depthStencilImageMemory = vkuAllocateImageMemory(
    device, physicalDeviceProps.memProps, depthStencilImage, true);
  VkImageSubresourceRange dRange = { VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1 };
  VkImageViewCreateInfo dImageViewInfo =
    vkiImageViewCreateInfo(depthStencilImage,
                           VK_IMAGE_VIEW_TYPE_2D,
                           depthStencilImageInfo.format,
                           { VK_COMPONENT_SWIZZLE_IDENTITY,
                             VK_COMPONENT_SWIZZLE_IDENTITY,
                             VK_COMPONENT_SWIZZLE_IDENTITY,
                             VK_COMPONENT_SWIZZLE_IDENTITY },
                           dRange);

  ASSERT_VK_SUCCESS(vkCreateImageView(
    device, &dImageViewInfo, nullptr, &depthStencilImageView));

  // renderpass 1:
  // attachments: 0 depth/stencil

  std::vector<VkAttachmentDescription> attachmentDescriptions;
  attachmentDescriptions.push_back(
    vkiAttachmentDescription(depthStencilFormat,
                             VK_SAMPLE_COUNT_1_BIT,
                             VK_ATTACHMENT_LOAD_OP_CLEAR,
                             VK_ATTACHMENT_STORE_OP_STORE,
                             VK_ATTACHMENT_LOAD_OP_CLEAR,
                             VK_ATTACHMENT_STORE_OP_STORE,
                             VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));

  VkAttachmentReference depthStencilAttachmentRef =
    vkiAttachmentReference(0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

  VkSubpassDescription subpassDesc =
    vkiSubpassDescription(VK_PIPELINE_BIND_POINT_GRAPHICS,
                          0,
                          nullptr,
                          0,
                          nullptr,
                          nullptr,
                          &depthStencilAttachmentRef,
                          0,
                          nullptr);

  std::vector<VkSubpassDependency> dependencies;

  dependencies.push_back(
    vkiSubpassDependency(VK_SUBPASS_EXTERNAL,
                         0,
                         VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                           VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                           VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
                         {}));

  VkRenderPassCreateInfo renderPassCreateInfo = vkiRenderPassCreateInfo(
    static_cast<uint32_t>(attachmentDescriptions.size()),
    attachmentDescriptions.data(),
    1,
    &subpassDesc,
    static_cast<uint32_t>(dependencies.size()),
    dependencies.data());

  ASSERT_VK_SUCCESS(
    vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &renderPassPre));

  // framebuffer for this pass
  {
    VkImageView attachments[1] = { depthStencilImageView };
    VkFramebufferCreateInfo createInfo =
      vkiFramebufferCreateInfo(renderPassPre,
                               1,
                               attachments,
                               swapchain->imageExtent.width,
                               swapchain->imageExtent.height,
                               1);
    ASSERT_VK_SUCCESS(
      vkCreateFramebuffer(device, &createInfo, nullptr, &framebufferPre));
  }

  attachmentDescriptions.clear();
  dependencies.clear();

  // renderpass 2: post process
  // attachments: 0 swapchain image, 1 depth stencil image

  attachmentDescriptions.push_back(
    vkiAttachmentDescription(swapchain->surfaceFormat.format,
                             VK_SAMPLE_COUNT_1_BIT,
                             VK_ATTACHMENT_LOAD_OP_CLEAR,
                             VK_ATTACHMENT_STORE_OP_STORE,
                             VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                             VK_ATTACHMENT_STORE_OP_DONT_CARE,
                             VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_PRESENT_SRC_KHR));

  attachmentDescriptions.push_back(
    vkiAttachmentDescription(depthStencilFormat,
                             VK_SAMPLE_COUNT_1_BIT,
                             VK_ATTACHMENT_LOAD_OP_LOAD,
                             VK_ATTACHMENT_STORE_OP_DONT_CARE,
                             VK_ATTACHMENT_LOAD_OP_LOAD,
                             VK_ATTACHMENT_STORE_OP_DONT_CARE,
                             VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_PRESENT_SRC_KHR));

  VkAttachmentReference colorAttachmentRef =
    vkiAttachmentReference(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  depthStencilAttachmentRef =
    vkiAttachmentReference(1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

  subpassDesc = vkiSubpassDescription(VK_PIPELINE_BIND_POINT_GRAPHICS,
                                      0,
                                      nullptr,
                                      1,
                                      &colorAttachmentRef,
                                      nullptr,
                                      &depthStencilAttachmentRef,
                                      0,
                                      nullptr);

  dependencies.push_back(vkiSubpassDependency(
    VK_SUBPASS_EXTERNAL,
    0,
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    {}));

  dependencies.push_back(
    vkiSubpassDependency(VK_SUBPASS_EXTERNAL,
                         0,
                         VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                           VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                           VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
                         {}));

  renderPassCreateInfo = vkiRenderPassCreateInfo(
    static_cast<uint32_t>(attachmentDescriptions.size()),
    attachmentDescriptions.data(),
    1,
    &subpassDesc,
    static_cast<uint32_t>(dependencies.size()),
    dependencies.data());

  ASSERT_VK_SUCCESS(vkCreateRenderPass(
    device, &renderPassCreateInfo, nullptr, &renderPassPost));

  // framebuffers for this pass
  framebuffersPost.resize(swapchain->imageCount);
  for (uint32_t i = 0; i < swapchain->imageCount; ++i) {
    VkImageView attachments[] = { swapchain->imageViews[i],
                                  depthStencilImageView };
    VkFramebufferCreateInfo createInfo =
      vkiFramebufferCreateInfo(renderPassPost,
                               2,
                               attachments,
                               swapchain->imageExtent.width,
                               swapchain->imageExtent.height,
                               1);

    ASSERT_VK_SUCCESS(
      vkCreateFramebuffer(device, &createInfo, nullptr, &framebuffersPost[i]));
  }

  // shader modules
  preFragmentShader = LoadShaderModule(device, "preSegment.frag.spv");
  preVertexShader = LoadShaderModule(device, "preSegment.vert.spv");

  preFanFragmentShader = LoadShaderModule(device, "preFan.frag.spv");
  preFanVertexShader = LoadShaderModule(device, "preFan.vert.spv");

  postFragmentShader = LoadShaderModule(device, "post.frag.spv");
  postVertexShader = LoadShaderModule(device, "post.vert.spv");

  // pipelines
  VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
  colorBlendAttachment.colorWriteMask =
    VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
    VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  colorBlendAttachment.blendEnable = VK_FALSE;

  prePipeline =
    GraphicsPipeline::GetBuilder()
      .SetDevice(device)
      .SetVertexShader(preVertexShader)
      .SetFragmentShader(preFragmentShader)
      .SetVertexBindings({ Vertex::GetBindingDescription() })
      .SetVertexAttributes(Vertex::GetAttributeDescriptions())
      .SetPrimitiveTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
      .SetViewports({ { 0.0f,
                        0.0f,
                        (float)swapchain->imageExtent.width,
                        (float)swapchain->imageExtent.height,
                        0.0f,
                        1.0f } })
      .SetScissors(
        { { { 0, 0 },
            { swapchain->imageExtent.width, swapchain->imageExtent.height } } })
      .SetColorBlendAttachments({ colorBlendAttachment })
      .SetDepthWriteEnable(VK_FALSE)
      .SetDepthTestEnable(VK_FALSE)
      .SetStencilTestEnable(VK_TRUE)
      .SetFront(vkiStencilOpState(VK_STENCIL_OP_INVERT,
                                  VK_STENCIL_OP_INVERT,
                                  {},
                                  VK_COMPARE_OP_NOT_EQUAL,
                                  1,
                                  1,
                                  0))
      .SetBack(vkiStencilOpState(VK_STENCIL_OP_INVERT,
                                 VK_STENCIL_OP_INVERT,
                                 {},
                                 VK_COMPARE_OP_NOT_EQUAL,
                                 1,
                                 1,
                                 0))
      .SetRenderPass(renderPassPre)
      .Build();

  preFanPipeline =
    GraphicsPipeline::GetBuilder()
      .SetDevice(device)
      .SetVertexShader(preFanVertexShader)
      .SetFragmentShader(preFanFragmentShader)
      .SetVertexBindings({ SimpleVertex::GetBindingDescription() })
      .SetVertexAttributes(SimpleVertex::GetAttributeDescriptions())
      .SetPrimitiveTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN)
      .SetDescriptorSetLayouts(
        { { { 0,
              VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
              1,
              VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT },
            { 1,
              VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              1,
              VK_SHADER_STAGE_FRAGMENT_BIT } } })
      .SetViewports({ { 0.0f,
                        0.0f,
                        (float)swapchain->imageExtent.width,
                        (float)swapchain->imageExtent.height,
                        0.0f,
                        1.0f } })
      .SetScissors(
        { { { 0, 0 },
            { swapchain->imageExtent.width, swapchain->imageExtent.height } } })
      .SetColorBlendAttachments({ colorBlendAttachment })
      .SetDepthWriteEnable(VK_FALSE)
      .SetDepthTestEnable(VK_FALSE)
      .SetStencilTestEnable(VK_TRUE)
      .SetFront(vkiStencilOpState(VK_STENCIL_OP_INVERT,
                                  VK_STENCIL_OP_INVERT,
                                  {},
                                  VK_COMPARE_OP_NOT_EQUAL,
                                  1,
                                  1,
                                  0))
      .SetBack(vkiStencilOpState(VK_STENCIL_OP_INVERT,
                                 VK_STENCIL_OP_INVERT,
                                 {},
                                 VK_COMPARE_OP_NOT_EQUAL,
                                 1,
                                 1,
                                 0))
      .SetRenderPass(renderPassPre)
      .Build();

  postPipeline =
    GraphicsPipeline::GetBuilder()
      .SetDevice(device)
      .SetVertexShader(postVertexShader)
      .SetFragmentShader(postFragmentShader)
      .SetVertexBindings({ SimpleVertex::GetBindingDescription() })
      .SetVertexAttributes(SimpleVertex::GetAttributeDescriptions())
      .SetViewports({ { 0.0f,
                        0.0f,
                        (float)swapchain->imageExtent.width,
                        (float)swapchain->imageExtent.height,
                        0.0f,
                        1.0f } })
      .SetScissors(
        { { { 0, 0 },
            { swapchain->imageExtent.width, swapchain->imageExtent.height } } })
      .SetColorBlendAttachments({ colorBlendAttachment })
      .SetDepthWriteEnable(VK_FALSE)
      .SetDepthTestEnable(VK_FALSE)
      .SetStencilTestEnable(VK_TRUE)
      .SetFront(vkiStencilOpState(VK_STENCIL_OP_KEEP,
                                  VK_STENCIL_OP_KEEP,
                                  {},
                                  VK_COMPARE_OP_NOT_EQUAL,
                                  1,
                                  1,
                                  0))
      .SetBack(vkiStencilOpState(VK_STENCIL_OP_KEEP,
                                 VK_STENCIL_OP_KEEP,
                                 {},
                                 VK_COMPARE_OP_NOT_EQUAL,
                                 1,
                                 1,
                                 0))
      .SetRenderPass(renderPassPost)
      .Build();

  // vertex buffer
  std::vector<float> floats = { -1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 0.0f,
                                -1.0f, 1.0f,  0.0f, 1.0f, -1.0f, 0.0f,
                                -1.0f, 1.0f,  0.0f, 1.0f, 1.0f,  0.0f };

  VkDeviceSize size = floats.size() * sizeof(float);

  vertexBuffer = vkuCreateBuffer(device,
                                 size,
                                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                 VK_SHARING_MODE_EXCLUSIVE,
                                 {});
  vertexBufferMemory =
    vkuAllocateBufferMemory(device,
                            physicalDeviceProps.memProps,
                            vertexBuffer,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                            true);

  vkuTransferData(device, vertexBufferMemory, 0, size, floats.data());

  dynamicVertexBuffer = vkuCreateBuffer(device,
                                        DYN_VERTEX_BUFFER_SIZE,
                                        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                        VK_SHARING_MODE_EXCLUSIVE,
                                        {});

  dynamicVertexBufferMemory =
    vkuAllocateBufferMemory(device,
                            physicalDeviceProps.memProps,
                            dynamicVertexBuffer,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                            true);

  hostDynamicVertexBuffer = (uint8_t*)malloc(DYN_VERTEX_BUFFER_SIZE);

  vkMapMemory(device,
              dynamicVertexBufferMemory,
              0,
              DYN_VERTEX_BUFFER_SIZE,
              0,
              (void**)&hostDynamicVertexBuffer);
}

Renderer::~Renderer()
{
  vkQueueWaitIdle(queue);
  destroyResources();
}

void
Renderer::destroyResources()
{
  // buffers
  vkDestroyBuffer(device, vertexBuffer, nullptr);
  vkFreeMemory(device, vertexBufferMemory, nullptr);

  vkDestroyBuffer(device, dynamicVertexBuffer, nullptr);
  vkFreeMemory(device, dynamicVertexBufferMemory, nullptr);

  // shader modules
  vkDestroyShaderModule(device, preFragmentShader, nullptr);
  vkDestroyShaderModule(device, preVertexShader, nullptr);
  vkDestroyShaderModule(device, preFanFragmentShader, nullptr);
  vkDestroyShaderModule(device, preFanVertexShader, nullptr);
  vkDestroyShaderModule(device, postFragmentShader, nullptr);
  vkDestroyShaderModule(device, postVertexShader, nullptr);

  // pipelines
  delete prePipeline;
  prePipeline = nullptr;
  delete preFanPipeline;
  preFanPipeline = nullptr;
  delete postPipeline;
  postPipeline = nullptr;

  // renderpasses and framebuffers
  for (auto fb : framebuffersPost) {
    vkDestroyFramebuffer(device, fb, nullptr);
  }
  vkDestroyRenderPass(device, renderPassPost, nullptr);
  vkDestroyFramebuffer(device, framebufferPre, nullptr);
  vkDestroyRenderPass(device, renderPassPre, nullptr);
  vkDestroyImageView(device, depthStencilImageView, nullptr);
  vkDestroyImage(device, depthStencilImage, nullptr);
  vkFreeMemory(device, depthStencilImageMemory, nullptr);
}

void
Renderer::recordCommandBuffer(uint32_t idx)
{
  ASSERT_VK_SUCCESS(
    vkWaitForFences(device, 1, &fences[idx], true, (uint64_t)-1));
  ASSERT_VK_SUCCESS(vkResetFences(device, 1, &fences[idx]));
  ASSERT_VK_SUCCESS(vkResetCommandBuffer(commandBuffers[idx], 0));

  VkCommandBufferBeginInfo beginInfo = vkiCommandBufferBeginInfo(nullptr);
  ASSERT_VK_SUCCESS(vkBeginCommandBuffer(commandBuffers[idx], &beginInfo));

  // prepass
  {
    VkClearValue clearValue = { 0.0f, 0.0f };

    VkRenderPassBeginInfo renderPassInfo =
      vkiRenderPassBeginInfo(renderPassPre,
                             framebufferPre,
                             { { 0, 0 }, swapchain->imageExtent },
                             1,
                             &clearValue);

    vkCmdBeginRenderPass(
      commandBuffers[idx], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkDeviceSize vbufferOffset =
      curDynVertexBufferPartition * DYN_VERTEX_BUFFER_PARTITION_SIZE;
    vkCmdBindVertexBuffers(
      commandBuffers[idx], 0, 1, &dynamicVertexBuffer, &vbufferOffset);

    vkCmdBindPipeline(commandBuffers[idx],
                      VK_PIPELINE_BIND_POINT_GRAPHICS,
                      prePipeline->pipeline);

    vkCmdDraw(commandBuffers[idx], numSegments * 3, 1, 0, 0);

    vbufferOffset =
      curDynVertexBufferPartition * DYN_VERTEX_BUFFER_PARTITION_SIZE +
      numSegments * sizeof(Segment);

    vkCmdBindVertexBuffers(
      commandBuffers[idx], 0, 1, &dynamicVertexBuffer, &vbufferOffset);

    vkCmdBindPipeline(commandBuffers[idx],
                      VK_PIPELINE_BIND_POINT_GRAPHICS,
                      preFanPipeline->pipeline);

    for (int i = 0; i < fanBegin.size(); ++i) {
      int numFanVerts = fanEnd[i] - fanBegin[i];
      vkCmdDraw(commandBuffers[idx], numFanVerts, 1, fanBegin[i], 0);
    }

    vkCmdEndRenderPass(commandBuffers[idx]);
  }

  // postpass
  {
    VkClearValue clearValues[] = { { 0.0f, 0.0f, 0.0f, 0.0f }, { 0.f, 0 } };
    VkRenderPassBeginInfo renderPassInfo =
      vkiRenderPassBeginInfo(renderPassPost,
                             framebuffersPost[idx],
                             { { 0, 0 }, swapchain->imageExtent },
                             2,
                             clearValues);

    vkCmdBeginRenderPass(
      commandBuffers[idx], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkDeviceSize vbufferOffset = 0;
    vkCmdBindVertexBuffers(
      commandBuffers[idx], 0, 1, &vertexBuffer, &vbufferOffset);

    vkCmdBindPipeline(commandBuffers[idx],
                      VK_PIPELINE_BIND_POINT_GRAPHICS,
                      postPipeline->pipeline);

    vkCmdDraw(commandBuffers[idx], 6, 1, 0, 0);

    vkCmdEndRenderPass(commandBuffers[idx]);
  }

  ASSERT_VK_SUCCESS(vkEndCommandBuffer(commandBuffers[idx]));
}

void
Renderer::drawFrame()
{
  uint32_t nextImageIdx = -1;
  ASSERT_VK_SUCCESS(vkAcquireNextImageKHR(device,
                                          swapchain->handle,
                                          UINT64_MAX,
                                          imageAvailableSemaphore,
                                          VK_NULL_HANDLE,
                                          &nextImageIdx));

  recordCommandBuffer(nextImageIdx);

  VkPipelineStageFlags waitStages[] = {
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
  };
  VkSubmitInfo submitInfo = vkiSubmitInfo(1,
                                          &imageAvailableSemaphore,
                                          waitStages,
                                          1,
                                          &commandBuffers[nextImageIdx],
                                          1,
                                          &renderFinishedSemaphore);
  ASSERT_VK_SUCCESS(vkQueueSubmit(queue, 1, &submitInfo, fences[nextImageIdx]));

  VkPresentInfoKHR presentInfo = vkiPresentInfoKHR(
    1, &renderFinishedSemaphore, 1, &swapchain->handle, &nextImageIdx, nullptr);
  ASSERT_VK_SUCCESS(vkQueuePresentKHR(queue, &presentInfo));

  // reset offsets / counts for the dynamic vertex buffer after each frame
  numSegments = 0;
  totalNumFanVerts = 0;
  fanBegin.clear();
  fanEnd.clear();

  curDynVertexBufferPartition = (curDynVertexBufferPartition + 1) % 2;
}

void
Renderer::pushSegments(const std::vector<Segment>& segments)
{
  size_t offset =
    curDynVertexBufferPartition * DYN_VERTEX_BUFFER_PARTITION_SIZE +
    numSegments * sizeof(Segment);
  size_t size = segments.size() * sizeof(Segment);

  ASSERT_TRUE(offset + size < DYN_VERTEX_BUFFER_SIZE);

  memcpy(hostDynamicVertexBuffer + offset, segments.data(), size);
  numSegments += static_cast<uint32_t>(segments.size());
}

void
Renderer::pushFan(const std::vector<glm::vec3>& fan)
{
  size_t offset =
    curDynVertexBufferPartition * DYN_VERTEX_BUFFER_PARTITION_SIZE +
    numSegments * sizeof(Segment) + totalNumFanVerts * sizeof(glm::vec3);

  size_t size = fan.size() * sizeof(glm::vec3);
  ASSERT_TRUE(offset + size < DYN_VERTEX_BUFFER_SIZE);

  memcpy(hostDynamicVertexBuffer + offset, fan.data(), size);

  fanBegin.push_back(totalNumFanVerts);
  totalNumFanVerts += static_cast<uint32_t>(fan.size());
  fanEnd.push_back(totalNumFanVerts);
}

void
Renderer::OnSwapchainReinitialized()
{
  destroyResources();
  createResources();
}
