#pragma once

#include <glm\glm.hpp>

#include "graphics_pipeline.h"
#include "vk_base.h"

struct Vertex
{
  glm::vec3 pos;
  glm::vec2 uv;

  static VkVertexInputBindingDescription GetBindingDescription()
  {
    VkVertexInputBindingDescription bindingDescription = {};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return bindingDescription;
  }

  static std::vector<VkVertexInputAttributeDescription>
  GetAttributeDescriptions()
  {
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions(2);

    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex, pos);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, uv);
    return attributeDescriptions;
  }
};

struct SimpleVertex
{
  glm::vec3 pos;

  static VkVertexInputBindingDescription GetBindingDescription()
  {
    VkVertexInputBindingDescription bindingDescription = {};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(SimpleVertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return bindingDescription;
  }

  static std::vector<VkVertexInputAttributeDescription>
  GetAttributeDescriptions()
  {
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions(1);

    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(SimpleVertex, pos);
    return attributeDescriptions;
  }
};

struct Renderer : VulkanBase
{
public:
  Renderer(VulkanWindow* window);
  ~Renderer();

  struct Segment
  {
    glm::vec3 p0;
    glm::vec2 uv0;

    glm::vec3 p1;
    glm::vec2 uv1;

    glm::vec3 p2;
    glm::vec2 uv2;
  };

  struct ContourRenderObj
  {
    std::vector<Segment> segments;
    std::vector<glm::vec3> fan;
  };

  void pushSegments(const std::vector<Segment>&);
  void pushFan(const std::vector<glm::vec3>&);

  void drawFrame();

private:
  virtual void OnSwapchainReinitialized();

  const int DYN_VERTEX_BUFFER_SIZE = 1024 * 1024 * 2;
  const int DYN_VERTEX_BUFFER_PARTITION_SIZE = DYN_VERTEX_BUFFER_SIZE / 2;
  uint32_t curDynVertexBufferPartition = 0;
  uint32_t numSegments = 0;
  uint32_t totalNumFanVerts = 0;

  std::vector<int> fanBegin;
  std::vector<int> fanEnd;

  GraphicsPipeline* postPipeline;
  VkShaderModule postVertexShader;
  VkShaderModule postFragmentShader;

  GraphicsPipeline* prePipeline;
  VkShaderModule preVertexShader;
  VkShaderModule preFragmentShader;

  GraphicsPipeline* preFanPipeline;
  VkShaderModule preFanVertexShader;
  VkShaderModule preFanFragmentShader;

  VkBuffer vertexBuffer;
  VkDeviceMemory vertexBufferMemory;

  VkBuffer dynamicVertexBuffer;
  VkDeviceMemory dynamicVertexBufferMemory;

  uint8_t* hostDynamicVertexBuffer; // will leak

  VkImage depthStencilImage = VK_NULL_HANDLE;
  VkImageView depthStencilImageView = VK_NULL_HANDLE;
  VkDeviceMemory depthStencilImageMemory = {};

  std::vector<VkFramebuffer> framebuffersPost = {};
  VkFramebuffer framebufferPre = VK_NULL_HANDLE;

  VkRenderPass renderPassPre = VK_NULL_HANDLE;
  VkRenderPass renderPassPost = VK_NULL_HANDLE;

  void recordCommandBuffer(uint32_t idx);

private:
  void createResources();
  void destroyResources();
};
