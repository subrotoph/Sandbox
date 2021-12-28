//  Copyright © 2021 Subph. All rights reserved.
//

#include "graphics_scene.hpp"

#include "../system.hpp"
#include "../resources/shader.hpp"

GraphicsScene::~GraphicsScene() {}
GraphicsScene::GraphicsScene() : m_pDevice(System::Device()) {}

void GraphicsScene::cleanup() { m_cleaner.flush("ComputeInterference"); }

void GraphicsScene::render(VkCommandBuffer cmdBuffer) {
    VkPipelineLayout pipelineLayout = m_pipelineLayout;
    VkPipeline       pipeline       = m_pPipeline->get();
    VkRenderPass     renderpass     = m_pRenderpass->get();
    VkFramebuffer    framebuffer    = m_pFrame->getFramebuffer();
    VkRect2D         scissor        = m_scissor;
    VkViewport       viewport       = m_viewport;
    
    VkDeviceSize offsets  = 0;
    VkBuffer vertexBuffer = m_pSphere->getVertexBuffer()->get();
    VkBuffer indexBuffer  = m_pSphere->getIndexBuffer()->get();
    uint32_t indexSize    = m_pSphere->getIndexSize();
    
    VkDescriptorSet cameraDescSet  = m_pDescriptor->getDescriptorSet(S0);
    VkDescriptorSet lightsDescSet = m_pDescriptor->getDescriptorSet(S1);
    VkDescriptorSet textureDescSet = m_pDescriptor->getDescriptorSet(S2);
    VkDescriptorSet heightmapDescSet = m_pDescriptor->getDescriptorSet(S3);
    VkDescriptorSet interferenceDescSet = m_pDescriptor->getDescriptorSet(S4);
    
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = System::Settings()->ClearColor;
    clearValues[1].depthStencil = System::Settings()->ClearDepth;
    
    VkRenderPassBeginInfo renderBeginInfo{};
    renderBeginInfo.sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderBeginInfo.clearValueCount = UINT32(clearValues.size());
    renderBeginInfo.pClearValues    = clearValues.data();
    renderBeginInfo.renderPass      = renderpass;
    renderBeginInfo.framebuffer     = framebuffer;
    renderBeginInfo.renderArea      = scissor;
    
    vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
    vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
    
    vkCmdBeginRenderPass(cmdBuffer, &renderBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineLayout, S0, 1, &cameraDescSet, 0, nullptr);
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineLayout, S1, 1, &lightsDescSet, 0, nullptr);
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineLayout, S2, 1, &textureDescSet, 0, nullptr);
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineLayout, S3, 1, &heightmapDescSet, 0, nullptr);
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineLayout, S4, 1, &interferenceDescSet, 0, nullptr);
    
    vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &vertexBuffer, &offsets);
    vkCmdBindIndexBuffer  (cmdBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    
    m_misc.model = m_pSphere->getMatrix();
    m_misc.isLight = 0;
    vkCmdPushConstants(cmdBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PCMisc), &m_misc);
    
    vkCmdDrawIndexed(cmdBuffer, indexSize, 1, 0, 0, 0);
    
    m_misc.isLight = 1;
    for (int i = 0; i < m_lights.total; i++) {
        m_misc.model = glm::translate(glm::mat4(1.0), glm::vec3(m_lights.position[i]));
        m_misc.model = glm::scale(m_misc.model, glm::vec3(0.2));
        vkCmdPushConstants(cmdBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PCMisc), &m_misc);
        
        vkCmdDrawIndexed(cmdBuffer, indexSize, 1, 0, 0, 0);
    }
    
    vkCmdEndRenderPass(cmdBuffer);
}

void GraphicsScene::setupShader() {
    LOG("GraphicsScene::setupShader");
    Shader* vertShader = new Shader(SPIRV_PATH + "main1d.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    Shader* fragShader = new Shader(SPIRV_PATH + "main1d.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    m_shaderStages = { vertShader->getShaderStageInfo(), fragShader->getShaderStageInfo() };
    m_cleaner.push([=](){ vertShader->cleanup(); fragShader->cleanup(); });
}

void GraphicsScene::setupInput() {
    LOG("GraphicsScene::setupInput");
    m_misc.reflectance = System::Settings()->Reflectance;
    m_lights.total = System::Settings()->TotalLight;
    
    m_pCameraBuffer = new Buffer();
    m_pCameraBuffer->setup(sizeof(UBCamera), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    m_pCameraBuffer->create();
    m_cleaner.push([=](){ m_pCameraBuffer->cleanup(); });
    
    m_pLightBuffer = new Buffer();
    m_pLightBuffer->setup(sizeof(UBLights), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    m_pLightBuffer->create();
    m_cleaner.push([=](){ m_pLightBuffer->cleanup(); });
    
    for (std::string path : getPBRTexturePaths()) {
        Image* pTexture = new Image();
        pTexture->setupForTexture(path);
        pTexture->createWithSampler();
        pTexture->cmdCopyRawDataToImage();
        pTexture->cmdTransitionToShaderR();
        m_pTextures.push_back(pTexture);
        m_cleaner.push([=](){ pTexture->cleanup(); });
    }
    
    m_pDescriptor->setupPointerBuffer(S0, B0, m_pCameraBuffer->getDescriptorInfo());
    m_pDescriptor->setupPointerBuffer(S1, B0, m_pLightBuffer->getDescriptorInfo());
    for (uint i = 0; i < m_pTextures.size(); i++)
        m_pDescriptor->setupPointerImage(S2, i, m_pTextures[i]->getDescriptorInfo());
    
    m_pDescriptor->update(S0);
    m_pDescriptor->update(S1);
    m_pDescriptor->update(S2);
    
    m_pSphere = new Mesh();
    m_pSphere->createSphere();
    m_pSphere->createVertexBuffer();
    m_pSphere->createIndexBuffer();
    m_pSphere->createVertexStateInfo();
    m_cleaner.push([=](){ m_pSphere->cleanup(); });
}

void GraphicsScene::setupCubemap(Image* cubemap, Image* env) {
    m_cleaner.push([=](){ cubemap->cleanup(); });
    m_cleaner.push([=](){ env->cleanup(); });
}

void GraphicsScene::updateLightInput() {
    Settings* settings = System::Settings();
    m_lights.radiance = settings->Radiance;
    m_lights.total = settings->TotalLight;
    m_lights.color = settings->LightColor;
    glm::vec2 distance = settings->Distance;
    long iteration = settings->Iteration;
    float interval = glm::radians(360.f/m_lights.total);
    for (int i = 0; i < m_lights.total; i++) {
        m_lights.position[i].z = distance.x;
        m_lights.position[i].x = sin(iteration / 100.f + i * interval) * distance.y;
        m_lights.position[i].y = cos(iteration / 100.f + i * interval) * distance.y;
    }
    m_pLightBuffer->fillBuffer(&m_lights, sizeof(UBLights));
}

void GraphicsScene::updateCameraInput(Camera* pCamera) {
    UInt2D size = m_pFrame->getSize();
    m_misc.viewPosition = pCamera->getPosition();
    m_cameraMatrix.view = pCamera->getViewMatrix();
    m_cameraMatrix.proj = pCamera->getProjection((float) size.width / size.height);
    
    m_pCameraBuffer->fillBuffer(&m_cameraMatrix, sizeof(UBCamera));
}

void GraphicsScene::updateHeightmapInput(Image *pHeightmapImage) {
    m_pHeightmapImage = pHeightmapImage;
    m_pHeightmapImage->cmdTransitionToShaderR();
    m_pDescriptor->setupPointerImage(S3, B0, m_pHeightmapImage->getDescriptorInfo());
    m_pDescriptor->update(S3);
}

void GraphicsScene::updateInterferenceInput(Image* pInterferenceImage) {
    m_pInterferenceImage = pInterferenceImage;
    m_pInterferenceImage->cmdTransitionToShaderR();
    m_pDescriptor->setupPointerImage(S4, B0, m_pInterferenceImage->getDescriptorInfo());
    m_pDescriptor->update(S4);
}

void GraphicsScene::createDescriptor() {
    LOG("GraphicsScene::createDescriptor");
    m_pDescriptor = new Descriptor();
    m_pDescriptor->setupLayout(S0);
    m_pDescriptor->addLayoutBindings(S0, B0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                     VK_SHADER_STAGE_VERTEX_BIT);
    m_pDescriptor->createLayout(S0);
    
    m_pDescriptor->setupLayout(S1);
    m_pDescriptor->addLayoutBindings(S1, B0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                     VK_SHADER_STAGE_FRAGMENT_BIT);
    m_pDescriptor->createLayout(S1);
    
    m_pDescriptor->setupLayout(S2);
    for (uint i = 0; i < USED_TEXTURE; i++) {
        m_pDescriptor->addLayoutBindings(S2, i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                       VK_SHADER_STAGE_FRAGMENT_BIT);
    }
    m_pDescriptor->createLayout(S2);
    
    m_pDescriptor->setupLayout(S3);
    m_pDescriptor->addLayoutBindings(S3, B0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   VK_SHADER_STAGE_FRAGMENT_BIT);
    m_pDescriptor->createLayout(S3);
    
    m_pDescriptor->setupLayout(S4);
    m_pDescriptor->addLayoutBindings(S4, B0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   VK_SHADER_STAGE_FRAGMENT_BIT);
    m_pDescriptor->createLayout(S4);
    
    m_pDescriptor->createPool();
    m_pDescriptor->allocate(S0);
    m_pDescriptor->allocate(S1);
    m_pDescriptor->allocate(S2);
    m_pDescriptor->allocate(S3);
    m_pDescriptor->allocate(S4);
    m_cleaner.push([=](){ m_pDescriptor->cleanup(); });
}

void GraphicsScene::createRenderpass() {
    m_pRenderpass = new Renderpass();
    m_pRenderpass->setupColorAttachment();
    m_pRenderpass->setupDepthAttachment();
    m_pRenderpass->setup();
    m_pRenderpass->create();
    m_cleaner.push([=](){ m_pRenderpass->cleanup(); });
}

void GraphicsScene::createPipelineLayout() {
    LOG("GraphicsScene::createPipelineLayout");
    VkDevice device = m_pDevice->getDevice();
    VECTOR<VkDescriptorSetLayout> descSetLayouts = {
        m_pDescriptor->getDescriptorLayout(S0),
        m_pDescriptor->getDescriptorLayout(S1),
        m_pDescriptor->getDescriptorLayout(S2),
        m_pDescriptor->getDescriptorLayout(S3),
        m_pDescriptor->getDescriptorLayout(S4)
    };
    
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.size = sizeof(PCMisc);
    pushConstantRange.offset = 0;
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = UINT32(descSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts    = descSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges    = &pushConstantRange;
    
    VkResult result = vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout);
    CHECK_VKRESULT(result, "failed to create pipeline layout!");
    m_cleaner.push([=](){ vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr); });
}

void GraphicsScene::createPipeline() {
    LOG("GraphicsScene::createPipeline");
    VkRenderPass renderpass = m_pRenderpass->get();
    VkPipelineLayout pipelineLayout = m_pipelineLayout;
    VECTOR<VkPipelineShaderStageCreateInfo> shaderStages = m_shaderStages;
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = m_pSphere->getVertexStateInfo();
    
    m_pPipeline = new Pipeline();
    m_pPipeline->setRenderpass(renderpass);
    m_pPipeline->setPipelineLayout(pipelineLayout);
    m_pPipeline->setShaderStages(shaderStages);
    m_pPipeline->setVertexInputInfo(vertexInputInfo);
    
    m_pPipeline->setupViewportInfo();
    m_pPipeline->setupInputAssemblyInfo();
    m_pPipeline->setupRasterizationInfo();
    m_pPipeline->setupMultisampleInfo();
    
    m_pPipeline->setupColorBlendInfo();
    m_pPipeline->setupBlendAttachment();
    
    m_pPipeline->setupDynamicInfo();
    m_pPipeline->setupDepthStencilInfo();

    m_pPipeline->createGraphicsPipeline();
    m_cleaner.push([=](){ m_pPipeline->cleanup(); });
}

void GraphicsScene::createFrame(UInt2D size) {
    LOG("GraphicsScene::createFrame");
    m_pFrame = new Frame(size);
    m_pFrame->createImageResource();
    m_pFrame->createDepthResource();
    m_pFrame->createFramebuffer(m_pRenderpass);
    m_cleaner.push([=](){ m_pFrame->cleanup(); });
    updateViewportScissor();
}

void GraphicsScene::recreateFrame(UInt2D size) {
    LOG("GraphicsScene::recreateFrame");
    m_pFrame->cleanup();
    m_pFrame->setSize(size);
    m_pFrame->createImageResource();
    m_pFrame->createDepthResource();
    m_pFrame->createFramebuffer(m_pRenderpass);
    updateViewportScissor();
}

void GraphicsScene::updateViewportScissor() {
    UInt2D extent = m_pFrame->getSize();
    m_viewport.x = 0.f;
    m_viewport.y = 0.f;
    m_viewport.width  = extent.width;
    m_viewport.height = extent.height;
    m_viewport.minDepth = 0.f;
    m_viewport.maxDepth = 1.f;
    m_scissor.offset = {0, 0};
    m_scissor.extent = extent;
}

Frame* GraphicsScene::getFrame() { return m_pFrame; }

std::string GraphicsScene::getTextureName() { return TEXTURE_NAMES[m_textureIdx] + "/" + TEXTURE_NAMES[m_textureIdx]; }
std::string GraphicsScene::getAlbedoTexturePath()    { return PBR_PATH + getTextureName() + TEXTURE_ALBEDO_PATH; }
std::string GraphicsScene::getAOTexturePath()        { return PBR_PATH + getTextureName() + TEXTURE_AO_PATH; }
std::string GraphicsScene::getMetallicTexturePath()  { return PBR_PATH + getTextureName() + TEXTURE_METALLIC_PATH; }
std::string GraphicsScene::getNormalTexturePath()    { return PBR_PATH + getTextureName() + TEXTURE_NORMAL_PATH; }
std::string GraphicsScene::getRoughnessTexturePath() { return PBR_PATH + getTextureName() + TEXTURE_ROUGHNESS_PATH; }
VECTOR<std::string> GraphicsScene::getPBRTexturePaths(){ return { getAlbedoTexturePath(), getAOTexturePath(), getMetallicTexturePath(), getNormalTexturePath(), getRoughnessTexturePath() }; }
