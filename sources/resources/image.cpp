//  Copyright © 2021 Subph. All rights reserved.
//

#include "image.hpp"

#include "../system.hpp"
#include "buffer.hpp"

#include "../extensions/ext_stb_image.h"

Image::~Image() {}
Image::Image() : m_pDevice(System::Device()),
                 m_imageInfo(GetDefaultImageCreateInfo()),
                 m_imageViewInfo(GetDefaultImageViewCreateInfo()),
                 m_imageLayout(VK_IMAGE_LAYOUT_UNDEFINED) {}

void Image::cleanup() { m_cleaner.flush("Image"); }

void Image::setupForDepth(UInt2D size) {
    LOG("Image::setupForDepth");
    m_imageInfo.extent = {size.width, size.height, 1};
    m_imageInfo.format = VK_FORMAT_D24_UNORM_S8_UINT;
    m_imageInfo.usage  = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                         VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                         VK_IMAGE_USAGE_SAMPLED_BIT;
    
    m_imageViewInfo.format = m_imageInfo.format;
    m_imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
}

void Image::setupForColor(UInt2D size) {
    LOG("Image::setupForColor");
    m_imageInfo.extent = {size.width, size.height, 1};
    m_imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    m_imageInfo.usage  = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                         VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                         VK_IMAGE_USAGE_SAMPLED_BIT;
    
    m_imageViewInfo.format = m_imageInfo.format;
    m_imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
}

void Image::setupForStorage(UInt2D size) {
    m_imageInfo.extent = {size.width, size.height, 1};
    m_imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    m_imageInfo.usage  = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                         VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                         VK_IMAGE_USAGE_STORAGE_BIT |
                         VK_IMAGE_USAGE_SAMPLED_BIT;

    m_imageViewInfo.format = m_imageInfo.format;
}

void Image::setupForSwapchain(VkImage image, VkFormat imageFormat) {
    LOG("Image::setupForSwapchain");
    m_image = image;
    m_imageInfo.format = imageFormat;
    m_imageViewInfo.format = imageFormat;
}

void Image::setupForTexture(const std::string filepath) {
    LOG("Image::setupForTexture");
    int width, height, channels;
    m_rawData = STBI::LoadImage(filepath, &width, &height, &channels);
    m_rawChannel = channels;
    UInt2D size = { (uint)width, (uint)height };
    setupForTexture(size);
}

void Image::setupForTexture(UInt2D size) {
    m_imageInfo.extent    = {size.width, size.height, 1};
    m_imageInfo.mipLevels = MaxMipLevel(size.width, size.height);
    m_imageInfo.format    = VK_FORMAT_R8G8B8A8_SRGB;
    m_imageInfo.usage     = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                            VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                            VK_IMAGE_USAGE_SAMPLED_BIT;
    
    m_imageViewInfo.format = m_imageInfo.format;
    m_imageViewInfo.subresourceRange.levelCount = m_imageInfo.mipLevels;
}

void Image::setupForHDRTexture(const std::string filepath) {
    LOG("Image::setupForHDRTexture");
    int width, height, channels;
    m_rawHDR = STBI::LoadHDR(filepath, &width, &height, &channels);
    m_rawChannel = channels;
    UInt2D size = { (uint)width, (uint)height };
    setupForHDRTexture(size);
}

void Image::setupForHDRTexture(UInt2D size) {
    m_imageInfo.extent    = {size.width, size.height, 1};
    m_imageInfo.mipLevels = MaxMipLevel(size.width, size.height);
    m_imageInfo.format    = VK_FORMAT_R32G32B32A32_SFLOAT;
    m_imageInfo.usage     = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                            VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                            VK_IMAGE_USAGE_STORAGE_BIT |
                            VK_IMAGE_USAGE_SAMPLED_BIT;
        
    m_imageViewInfo.format = m_imageInfo.format;
    m_imageViewInfo.subresourceRange.levelCount = m_imageInfo.mipLevels;
}

void Image::setupForCubemap(UInt2D size) {
    m_imageInfo.extent       = {size.width, size.height, 1};
    m_imageInfo.mipLevels    = 1;
    m_imageInfo.arrayLayers  = 6;
    m_imageInfo.format       = VK_FORMAT_R32G32B32A32_SFLOAT;
    m_imageInfo.flags        = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    m_imageInfo.usage        = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                               VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                               VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                               VK_IMAGE_USAGE_STORAGE_BIT |
                               VK_IMAGE_USAGE_SAMPLED_BIT;
      
    m_imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    m_imageViewInfo.format   = m_imageInfo.format;
    m_imageViewInfo.subresourceRange.levelCount = 1;
    m_imageViewInfo.subresourceRange.layerCount = 6;
}

void Image::create() {
    createImage();
    allocateImageMemory();
    createImageViews();
}

void Image::createWithSampler() {
    createImage();
    allocateImageMemory();
    createImageViews();
    createSampler();
}

void Image::createForSwapchain() {
    createImageViews();
}

void Image::createImage() {
    LOG("Image::createImage");
    VkDevice device = m_pDevice->getDevice();
    VkResult result = vkCreateImage(device, &m_imageInfo, nullptr, &m_image);
    CHECK_VKRESULT(result, "failed to create image!");
    m_cleaner.push([=](){ vkDestroyImage(device, m_image, nullptr); });
}

void Image::createImageViews() {
    LOG("Image::createImageViews");
    VkDevice device = m_pDevice->getDevice();
    uint mipLevels = m_imageInfo.mipLevels;
    m_imageViews.resize(mipLevels);
    m_imageViewInfo.image = m_image;
    for (int i = 0; i < mipLevels; i++) {
        m_imageViewInfo.subresourceRange.baseMipLevel = i;
        m_imageViewInfo.subresourceRange.levelCount = mipLevels - i;
        VkResult result = vkCreateImageView(device, &m_imageViewInfo, nullptr, &m_imageViews[i]);
        CHECK_VKRESULT(result, "failed to create image views!");
        m_cleaner.push([=](){ vkDestroyImageView(device, m_imageViews.back(), nullptr); m_imageViews.pop_back(); });
    }
    m_imageViewInfo.subresourceRange.baseMipLevel = 0;
    m_imageViewInfo.subresourceRange.levelCount = mipLevels;
}

void Image::allocateImageMemory() {
    LOG("Image::allocateImageMemory");
    Device*  pDevice = m_pDevice;
    VkDevice device  = m_pDevice->getDevice();
    VkImage  image   = m_image;
    
    VkMemoryRequirements memoryRequirements;
    vkGetImageMemoryRequirements(device, image, &memoryRequirements);
    uint32_t memoryTypeIndex = pDevice->findMemoryTypeIndex(memoryRequirements.memoryTypeBits,
                                                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memoryRequirements.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;
    
    VkResult result = vkAllocateMemory(device, &allocInfo, nullptr, &m_imageMemory);
    CHECK_VKRESULT(result, "failed to allocate image memory!");
    m_cleaner.push([=](){ vkFreeMemory(device, m_imageMemory, nullptr); });
    vkBindImageMemory(device, image, m_imageMemory, 0);
}

void Image::createSampler() {
    LOG("Image::createSampler");
    VkDevice device    = m_pDevice->getDevice();
    float    mipLevels = m_imageInfo.mipLevels;
    
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter    = VK_FILTER_LINEAR;
    samplerInfo.minFilter    = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy    = 16.0f;
    samplerInfo.borderColor      = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.compareEnable    = VK_FALSE;
    samplerInfo.compareOp        = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode       = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias       = 0.0f;
    samplerInfo.minLod           = 0.0f;
    samplerInfo.maxLod           = mipLevels;
    
    VkResult result = vkCreateSampler(device, &samplerInfo, nullptr, &m_sampler);
    CHECK_VKRESULT(result, "failed to create texture sampler!");
    m_cleaner.push([=](){ vkDestroySampler(device, m_sampler, nullptr); });
}

void Image::cmdCopyRawDataToImage() {
    LOG("Image::copyRawDataToImage");
    Buffer *tempBuffer = new Buffer();
    tempBuffer->setup(getDeviceSize(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    tempBuffer->create();
    tempBuffer->fillBufferFull(m_rawData);
    
    Commander* pCommander = System::Commander();
    VkCommandBuffer cmdBuffer = pCommander->createCommandBuffer();
    pCommander->beginSingleTimeCommands(cmdBuffer);
    cmdTransitionToTransferDst(cmdBuffer);
    cmdCopyBufferToImage(cmdBuffer, tempBuffer->get());
    cmdGenerateMipmaps(cmdBuffer);
    pCommander->endSingleTimeCommands(cmdBuffer);
    tempBuffer->cleanup();
}

void Image::cmdClearColorImage(VkClearColorValue clearColor) {
    LOG("Image::cmdClearColorImage");
    Commander*      pCommander = System::Commander();
    VkCommandBuffer cmdBuffer  = pCommander->createCommandBuffer();
    pCommander->beginSingleTimeCommands(cmdBuffer);
    cmdTransitionToTransferDst(cmdBuffer);
    vkCmdClearColorImage(cmdBuffer, m_image, m_imageLayout,
                         &clearColor, 1, &m_imageViewInfo.subresourceRange);
    pCommander->endSingleTimeCommands(cmdBuffer);
}

void Image::cmdCopyImageToImage(VkCommandBuffer cmdBuffer, Image* pSrcImage) {
    cmdCopyImageToImage(cmdBuffer, pSrcImage, m_imageInfo.extent);
}

void Image::cmdCopyImageToImage(VkCommandBuffer cmdBuffer, Image* pSrcImage, VkExtent3D extent, uint srcMipLevel, uint dstMipLevel) {
    VkImage               srcImage         = pSrcImage->getImage();
    VkImageLayout         srcImageLayout   = pSrcImage->getImageLayout();
    VkImageViewCreateInfo srcImageViewInfo = pSrcImage->getImageViewInfo();
    VkImage               dstImage         = m_image;
    VkImageLayout         dstImageLayout   = m_imageLayout;
    VkImageViewCreateInfo dstImageViewInfo = m_imageViewInfo;
    
    VkImageCopy region{};
    region.srcOffset = {0, 0, 0};
    region.srcSubresource.aspectMask     = srcImageViewInfo.subresourceRange.aspectMask;
    region.srcSubresource.baseArrayLayer = srcImageViewInfo.subresourceRange.baseArrayLayer;
    region.srcSubresource.layerCount     = srcImageViewInfo.subresourceRange.layerCount;
    region.srcSubresource.mipLevel       = srcMipLevel;
    
    region.dstOffset = {0, 0, 0};
    region.dstSubresource.aspectMask     = dstImageViewInfo.subresourceRange.aspectMask;
    region.dstSubresource.baseArrayLayer = dstImageViewInfo.subresourceRange.baseArrayLayer;
    region.dstSubresource.layerCount     = dstImageViewInfo.subresourceRange.layerCount;
    region.dstSubresource.mipLevel       = dstMipLevel;
    region.extent = extent;
    
    vkCmdCopyImage(cmdBuffer,
                   srcImage, srcImageLayout,
                   dstImage, dstImageLayout,
                   1, &region);
}

void Image::cmdCopyBufferToImage(VkCommandBuffer cmdBuffer, VkBuffer buffer) {
    LOG("Image::cmdCopyBufferToImage");
    VkImage               image         = m_image;
    VkImageCreateInfo     imageInfo     = m_imageInfo;
    VkImageViewCreateInfo imageViewInfo = m_imageViewInfo;
    
    VkBufferImageCopy region{};
    region.bufferOffset      = 0;
    region.bufferRowLength   = 0;
    region.bufferImageHeight = 0;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = imageInfo.extent;
    
    region.imageSubresource.aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel        = 0;
    region.imageSubresource.baseArrayLayer  = 0;
    region.imageSubresource.layerCount      = imageViewInfo.subresourceRange.layerCount;
    
    vkCmdCopyBufferToImage(cmdBuffer,
                           buffer,
                           image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1, &region);
}

void Image::cmdGenerateMipmaps(VkCommandBuffer cmdBuffer) {
    LOG("Image::cmdGenerateMipmaps");
    VkPhysicalDevice      physicalDevice = m_pDevice->getPhysicalDevice();;
    VkImage               image          = m_image;
    VkImageCreateInfo     imageInfo      = m_imageInfo;
    VkImageViewCreateInfo imageViewInfo  = m_imageViewInfo;
    uint32_t layerCount = imageViewInfo.subresourceRange.layerCount;
    
    // Check if image format supports linear blitting
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(physicalDevice, imageInfo.format, &formatProperties);
    if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        throw std::runtime_error("texture image format does not support linear blitting!");
    }
    
    VkImageMemoryBarrier barrier = GetDefaultImageMemoryBarrier();
    barrier.image = image;
    barrier.subresourceRange.layerCount = layerCount;

    int32_t mipWidth  = imageInfo.extent.width;
    int32_t mipHeight = imageInfo.extent.height;
    
    for (uint32_t i = 1; i < imageInfo.mipLevels; i++) {
        int32_t halfMipWidth  = ceil(mipWidth /2);
        int32_t halfMipHeight = ceil(mipHeight/2);
        
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(cmdBuffer,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                             0, nullptr,
                             0, nullptr,
                             1, &barrier);
        
        VkImageBlit blit{};
        blit.srcOffsets[0] = { 0, 0, 0 };
        blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
        blit.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel       = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount     = layerCount;
        
        blit.dstOffsets[0] = { 0, 0, 0 };
        blit.dstOffsets[1] = { halfMipWidth, halfMipHeight, 1 };
        blit.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel       = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount     = layerCount;
        
        vkCmdBlitImage(cmdBuffer,
                       image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1, &blit,
                       VK_FILTER_LINEAR);
        
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(cmdBuffer,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                             0, nullptr,
                             0, nullptr,
                             1, &barrier);
        
        mipWidth  = halfMipWidth;
        mipHeight = halfMipHeight;
    }
}

void Image::cmdTransitionToShaderR(VkCommandBuffer cmdBuffer) {
    cmdChangeLayout(cmdBuffer,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_ACCESS_SHADER_READ_BIT,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
}
void Image::cmdTransitionToPresent(VkCommandBuffer cmdBuffer) {
    cmdChangeLayout(cmdBuffer,
                    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
}
void Image::cmdTransitionToStorageW(VkCommandBuffer cmdBuffer) {
    cmdChangeLayout(cmdBuffer,
                    VK_IMAGE_LAYOUT_GENERAL,
                    VK_ACCESS_SHADER_WRITE_BIT,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
}
void Image::cmdTransitionToStorageRW(VkCommandBuffer cmdBuffer) {
    cmdChangeLayout(cmdBuffer,
                    VK_IMAGE_LAYOUT_GENERAL,
                    VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
}
void Image::cmdTransitionToTransferDst(VkCommandBuffer cmdBuffer) {
    cmdChangeLayout(cmdBuffer,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_ACCESS_TRANSFER_WRITE_BIT,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT);
}
void Image::cmdTransitionToTransferSrc(VkCommandBuffer cmdBuffer) {
    cmdChangeLayout(cmdBuffer,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    VK_ACCESS_TRANSFER_READ_BIT,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT);
}

void Image::cmdChangeLayout(VkCommandBuffer cmdBuffer,
                            VkImageLayout newLayout,
                            VkAccessFlags dstAccess,
                            VkPipelineStageFlags srcStage,
                            VkPipelineStageFlags dstStage) {
    VkImageMemoryBarrier barrier = GetDefaultImageMemoryBarrier();
    barrier.image         = m_image;
    barrier.oldLayout     = m_imageLayout;
    barrier.newLayout     = newLayout;
    barrier.dstAccessMask = dstAccess;
    barrier.subresourceRange = m_imageViewInfo.subresourceRange;
    
    m_imageLayout = newLayout;
    vkCmdPipelineBarrier(cmdBuffer,
                         srcStage, dstStage, 0,
                         0, nullptr,
                         0, nullptr,
                         1, &barrier);
}

void Image::cmdTransitionToShaderR    () { cmdCall(&Image::cmdTransitionToShaderR);     }
void Image::cmdTransitionToPresent    () { cmdCall(&Image::cmdTransitionToPresent);     }
void Image::cmdTransitionToStorageW   () { cmdCall(&Image::cmdTransitionToStorageW);    }
void Image::cmdTransitionToStorageRW  () { cmdCall(&Image::cmdTransitionToStorageRW);   }
void Image::cmdTransitionToTransferDst() { cmdCall(&Image::cmdTransitionToTransferDst); }
void Image::cmdTransitionToTransferSrc() { cmdCall(&Image::cmdTransitionToTransferSrc); }

VkDescriptorImageInfo* Image::getDescriptorInfo() {
    uint mipLevels = m_imageInfo.mipLevels;
    m_descriptorInfos.resize(mipLevels);
    for (int i = 0; i < mipLevels; i++) {
        m_descriptorInfos[i].imageLayout = m_imageLayout;
        m_descriptorInfos[i].imageView   = m_imageViews[i];
        m_descriptorInfos[i].sampler     = m_sampler;
    }
    return m_descriptorInfos.data();
}

VkImageView     Image::getImageView  (uint idx) { return m_imageViews[idx]; }
VkImage         Image::getImage      () { return m_image;       }
VkDeviceMemory  Image::getImageMemory() { return m_imageMemory; }
VkSampler       Image::getSampler    () { return m_sampler;     }
uint            Image::getRawChannel () { return m_rawChannel;  }
uint            Image::getChannelSize() { return GetChannelSize(m_imageInfo.format); }
uint            Image::getMipLevels  () { return m_imageInfo.mipLevels; }
UInt2D          Image::getImageSize  () { return {m_imageInfo.extent.width, m_imageInfo.extent.height}; }
VkDeviceSize    Image::getDeviceSize () { return m_imageInfo.extent.width * m_imageInfo.extent.height * getChannelSize() * m_imageInfo.arrayLayers; }

VkImageLayout         Image::getImageLayout()   { return m_imageLayout; }
VkImageCreateInfo     Image::getImageInfo()     { return m_imageInfo; }
VkImageViewCreateInfo Image::getImageViewInfo() { return m_imageViewInfo; }

unsigned char* Image::getRawData() { return m_rawData; }
float        * Image::getRawHDR()  { return m_rawHDR;  }

void Image::setMipLevels(uint mipLevels) { m_imageInfo.mipLevels = mipLevels; }
void Image::setImageLayout(VkImageLayout imageLayout) { m_imageLayout = imageLayout; }
void Image::setImageFormat(VkFormat format) {
    m_imageInfo.format = format;
    m_imageViewInfo.format = format;
}


// Private ==================================================

void Image::cmdCall(void (Image::*cmdFunc)(VkCommandBuffer)) {
    Commander*      pCommander = System::Commander();
    VkCommandBuffer cmdBuffer  = pCommander->createCommandBuffer();
    pCommander->beginSingleTimeCommands(cmdBuffer);
    (this->*cmdFunc)(cmdBuffer);
    pCommander->endSingleTimeCommands(cmdBuffer);
}

uint32_t Image::MaxMipLevel(int width, int height) {
    return fmin(UINT32(std::floor(std::log2(std::max(width, height)))) + 1, 7);
}

unsigned int Image::GetChannelSize(VkFormat format) {
    switch (format) {
        case VK_FORMAT_R8G8B8_SRGB  : return 3; break;
        case VK_FORMAT_R8G8B8A8_SRGB: return 4; break;
        case VK_FORMAT_R32G32B32_SFLOAT: return 12; break;
        case VK_FORMAT_R32G32B32A32_SFLOAT: return 16; break;
        default: return 0; break;
    }
}

VkImageCreateInfo Image::GetDefaultImageCreateInfo() {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType     = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.extent.depth  = 1;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    return imageInfo;
}

VkImageViewCreateInfo Image::GetDefaultImageViewCreateInfo() {
    VkImageViewCreateInfo imageViewInfo{};
    imageViewInfo.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewInfo.subresourceRange.baseMipLevel   = 0;
    imageViewInfo.subresourceRange.levelCount     = 1;
    imageViewInfo.subresourceRange.baseArrayLayer = 0;
    imageViewInfo.subresourceRange.layerCount     = 1;
    return imageViewInfo;
}

VkImageMemoryBarrier Image::GetDefaultImageMemoryBarrier() {
    VkImageMemoryBarrier barrier{};
    barrier.sType     = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = 0;
    return barrier;
}
