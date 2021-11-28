//  Copyright © 2021 Subph. All rights reserved.
//

#include "screenspace_pipeline.hpp"

#include "../system.hpp"
#include "../resources/shader.hpp"


ScreenSpacePipeline::~ScreenSpacePipeline() {}
ScreenSpacePipeline::ScreenSpacePipeline() : m_pDevice(System::Device()) {}

void ScreenSpacePipeline::cleanup() { m_cleaner.flush("ScreenSpacePipeline"); }

void ScreenSpacePipeline::render(VkCommandBuffer cmdBuffer) {
    VkPipeline pipeline = m_pPipeline->get();
    VkRenderPass renderpass = m_pRenderpass->get();
    VkFramebuffer framebuffer = m_pFrame->getFramebuffer();
    UInt2D extent = m_pFrame->getExtent2D();
    
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {0.1f, 0.1f, 0.1f, 1.0f};
    clearValues[1].depthStencil = {1.0f, 0};
    
    VkViewport viewport{};
    viewport.x = 0.f;
    viewport.y = 0.f;
    viewport.width  = extent.width;
    viewport.height = extent.height;
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;
    
    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = extent;
    
    vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
    vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
    
    VkRenderPassBeginInfo renderBeginInfo{};
    renderBeginInfo.sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderBeginInfo.clearValueCount = UINT32(clearValues.size());
    renderBeginInfo.pClearValues    = clearValues.data();
    renderBeginInfo.renderPass      = renderpass;
    renderBeginInfo.framebuffer     = framebuffer;
    renderBeginInfo.renderArea.extent = extent;
    renderBeginInfo.renderArea.offset = {0,0};
    
    vkCmdBeginRenderPass(cmdBuffer, &renderBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdDraw(cmdBuffer, 3, 1, 0, 0);
    
    m_pGUI->renderGUI(cmdBuffer);
    
    vkCmdEndRenderPass(cmdBuffer);
}

void ScreenSpacePipeline::setupShader() {
    LOG("ScreenSpacePipeline::setupShader");
    Shader* vertShader = new Shader(SPIRV_PATH + "swapchain.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    Shader* fragShader = new Shader(SPIRV_PATH + "swapchain.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    m_shaderStages = { vertShader->getShaderStageInfo(), fragShader->getShaderStageInfo() };
    m_cleaner.push([=](){ vertShader->cleanup(); fragShader->cleanup(); });
}

void ScreenSpacePipeline::createRenderpass() {
    VkSurfaceFormatKHR surfaceFormat = m_pDevice->getSurfaceFormat();
    m_pRenderpass = new Renderpass();
    m_pRenderpass->setupColorAttachment(surfaceFormat.format);
    m_pRenderpass->setup();
    m_pRenderpass->create();
    m_cleaner.push([=](){ m_pRenderpass->cleanup(); });
}

void ScreenSpacePipeline::createPipelineLayout() {
    LOG("ScreenSpacePipeline::createPipelineLayout");
    VkDevice device = m_pDevice->getDevice();
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    VkResult result = vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout);
    CHECK_VKRESULT(result, "failed to create pipeline layout!");
    m_cleaner.push([=](){ vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr); });
}

void ScreenSpacePipeline::createPipeline() {
    LOG("ScreenSpacePipeline::createPipeline");
    VkRenderPass renderpass = m_pRenderpass->get();
    VkPipelineLayout pipelineLayout = m_pipelineLayout;
    VECTOR<VkPipelineShaderStageCreateInfo> shaderStages = m_shaderStages;
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    
    m_pPipeline = new Pipeline();
    m_pPipeline->setRenderpass(renderpass);
    m_pPipeline->setPipelineLayout(pipelineLayout);
    m_pPipeline->setShaderStages(shaderStages);
    m_pPipeline->setVertexInputInfo(vertexInputInfo);
    
    m_pPipeline->setupViewportInfo();
    m_pPipeline->setupInputAssemblyInfo();
    m_pPipeline->setupRasterizationInfo();
    m_pPipeline->setupMultisampleInfo();
    
    m_pPipeline->disableBlendAttachment();
    m_pPipeline->setupColorBlendInfo();
    
    m_pPipeline->setupDynamicInfo();

    m_pPipeline->createGraphicsPipeline();
    m_cleaner.push([=](){ m_pPipeline->cleanup(); });
}

void ScreenSpacePipeline::createGUI(Window* pWindow) {
    Renderpass* pRenderpass = m_pRenderpass;
    m_pGUI = new GUI();
    m_pGUI->initGUI(pWindow, pRenderpass);
    m_cleaner.push([=](){ m_pGUI->cleanupGUI(); });
}

void ScreenSpacePipeline::setFrame(Frame *pFrame) { m_pFrame = pFrame; }

Renderpass* ScreenSpacePipeline::getRenderpass() { return m_pRenderpass; }


