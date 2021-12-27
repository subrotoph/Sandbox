//  Copyright © 2021 Subph. All rights reserved.
//

#include "compute_interference.hpp"

#include "../system.hpp"
#include "../resources/shader.hpp"

#define WORKGROUP_SIZE_X 128

ComputeInterference::~ComputeInterference() {}
ComputeInterference::ComputeInterference() {}

void ComputeInterference::cleanup() { m_cleaner.flush("ComputeInterference"); }

void ComputeInterference::setupShader() {
    LOG("ComputeInterference::setupShader");
    Shader* compShader = new Shader(SPIRV_PATH + "interference1d.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);
    m_shaderStage = compShader->getShaderStageInfo();
    m_cleaner.push([=](){ compShader->cleanup(); });
}

void ComputeInterference::setupInput() {
    m_misc.opdSample = System::Settings()->OPDSample;
    m_misc.rSample   = System::Settings()->RSample;
}

void ComputeInterference::setupOutput() {
    LOG("ComputeInterference::setupOutput");
    m_pOutputImage = new Image();
    m_pOutputImage->setupForStorage({m_misc.opdSample, m_misc.rSample});
    m_pOutputImage->createWithSampler();
    m_pOutputImage->cmdTransitionToStorageW();
    m_cleaner.push([=](){ m_pOutputImage->cleanup(); });
    
    m_pDescriptor->setupPointerImage(S0, B0, m_pOutputImage->getDescriptorInfo());
    m_pDescriptor->update(S0);
}

void ComputeInterference::createDescriptor() {
    LOG("ComputeInterference::createDescriptor");
    m_pDescriptor = new Descriptor();
    
    m_pDescriptor->setupLayout(S0);
    m_pDescriptor->addLayoutBindings(S0, B0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                     VK_SHADER_STAGE_COMPUTE_BIT);
    m_pDescriptor->createLayout(S0);
    m_pDescriptor->createPool();

    m_pDescriptor->allocate(S0);
    m_cleaner.push([=](){ m_pDescriptor->cleanup(); });
}

void ComputeInterference::createPipelineLayout() {
    LOG("ComputeInterference::createPipelineLayout");
    VkDevice device = System::Device()->getDevice();
    VkDescriptorSetLayout descSetLayout = m_pDescriptor->getDescriptorLayout(S0);
    
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.size = sizeof(PCMisc);
    pushConstantRange.offset = 0;
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts    = &descSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges    = &pushConstantRange;
    
    VkResult result = vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout);
    CHECK_VKRESULT(result, "failed to create pipeline layout!");
    m_cleaner.push([=](){ vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr); });
}

void ComputeInterference::createPipeline() {
    LOG("ComputeInterference::createPipeline");
    VkPipelineLayout pipelineLayout = m_pipelineLayout;
    VkPipelineShaderStageCreateInfo shaderStage = m_shaderStage;
    
    m_pPipeline = new Pipeline();
    m_pPipeline->setPipelineLayout(pipelineLayout);
    m_pPipeline->setShaderStages({shaderStage});
    m_pPipeline->createComputePipeline();
    m_cleaner.push([=](){ m_pPipeline->cleanup(); });
}

void ComputeInterference::dispatch() {
    Commander* pCommander = System::Commander();
    VkCommandBuffer cmdBuffer = pCommander->createCommandBuffer();
    pCommander->beginSingleTimeCommands(cmdBuffer);
    dispatch(cmdBuffer);
    pCommander->endSingleTimeCommands(cmdBuffer);
}

void ComputeInterference::dispatch(VkCommandBuffer cmdBuffer) {
    LOG("ComputeInterference::dispatch");
    VkPipelineLayout pipelineLayout = m_pipelineLayout;
    VkPipeline       pipeline = m_pPipeline->get();
    VkDescriptorSet  descSet  = m_pDescriptor->getDescriptorSet(S0);
    PCMisc           misc     = m_misc;
    
    vkCmdPushConstants(cmdBuffer, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                       0, sizeof(PCMisc), &misc);
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                            pipelineLayout, 0, 1, &descSet, 0, nullptr);
    
    vkCmdDispatch(cmdBuffer, misc.opdSample / WORKGROUP_SIZE_X + 1, misc.rSample, 1);
    
    m_pOutputImage->cmdTransitionToTransferSrc(cmdBuffer);
}

Image* ComputeInterference::copyOutputImage() {
    UInt2D imageSize = m_pOutputImage->getImageSize();
    Image* imageCopy = new Image();
    imageCopy->setupForStorage(imageSize);
    imageCopy->createWithSampler();
    
    Commander* pCommander = System::Commander();
    VkCommandBuffer cmdBuffer = pCommander->createCommandBuffer();
    pCommander->beginSingleTimeCommands(cmdBuffer);
    imageCopy->cmdTransitionToTransferDst(cmdBuffer);
    imageCopy->cmdCopyImageToImage(cmdBuffer, m_pOutputImage);
    pCommander->endSingleTimeCommands(cmdBuffer);
    
    return imageCopy;
}

