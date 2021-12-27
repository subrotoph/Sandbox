//  Copyright © 2021 Subph. All rights reserved.
//

#pragma once

#include "../include.h"
#include "device.hpp"

class Image {
    
public:
    ~Image();
    Image();

    void cleanup();
    
    void setupForDepth      (UInt2D size);
    void setupForColor      (UInt2D size);
    void setupForStorage    (UInt2D size);
    void setupForSwapchain  (VkImage image, VkFormat imageFormat);
    
    void setupForTexture    (const std::string filepath);
    void setupForTexture    (UInt2D size);
    void setupForHDRTexture (const std::string filepath);
    void setupForHDRTexture (UInt2D size);
    void setupForCubemap    (UInt2D size);
    
    void create             ();
    void createWithSampler  ();
    void createForSwapchain ();
     
    void createImage        ();
    void createImageView    ();
    void allocateImageMemory();
    void createSampler      ();
    
    void cmdCopyRawDataToImage();
    void cmdClearColorImage   (VkClearColorValue clearColor = {0., 0., 0., 1.});
    
    void cmdTransitionToShaderR();
    void cmdTransitionToShaderR(VkCommandBuffer cmdBuffer);
    void cmdTransitionToPresent();
    void cmdTransitionToPresent(VkCommandBuffer cmdBuffer);
    void cmdTransitionToStorageW();
    void cmdTransitionToStorageW(VkCommandBuffer cmdBuffer);
    void cmdTransitionToStorageRW();
    void cmdTransitionToStorageRW(VkCommandBuffer cmdBuffer);
    void cmdTransitionToTransferDst();
    void cmdTransitionToTransferDst(VkCommandBuffer cmdBuffer);
    void cmdTransitionToTransferSrc();
    void cmdTransitionToTransferSrc(VkCommandBuffer cmdBuffer);
    
    void cmdChangeLayout(VkCommandBuffer cmdBuffer,
                         VkImageLayout newLayout,
                         VkAccessFlags dstAccess,
                         VkPipelineStageFlags srcStage,
                         VkPipelineStageFlags dstStage);
    
    void cmdCopyImageToImage (VkCommandBuffer cmdBuffer, Image* srcImage);
    void cmdCopyBufferToImage(VkCommandBuffer cmdBuffer, VkBuffer buffer);
    void cmdGenerateMipmaps  (VkCommandBuffer cmdBuffer);
    
    VkImage          getImage          ();
    VkImageView      getImageView      ();
    VkDeviceMemory   getImageMemory    ();
    UInt2D           getImageSize      ();
    VkDeviceSize     getDeviceSize     ();
    VkSampler        getSampler        ();
    uint             getRawChannel     ();
    uint             getChannelSize    ();
    VkDescriptorImageInfo* getDescriptorInfo();
    
    VkImageLayout         getImageLayout();
    VkImageCreateInfo     getImageInfo();
    VkImageViewCreateInfo getImageViewInfo();
    
    unsigned char* getRawData();
    float        * getRawHDR();
    
    void setImageLayout(VkImageLayout imageLayout);
    
private:
    Cleaner m_cleaner;
    Device* m_pDevice;
    
    uint m_rawChannel;
    unsigned char* m_rawData;
    float        * m_rawHDR;

    VkImage          m_image          = VK_NULL_HANDLE;
    VkImageView      m_imageView      = VK_NULL_HANDLE;
    VkDeviceMemory   m_imageMemory    = VK_NULL_HANDLE;
    
    VkImageLayout         m_imageLayout;
    VkImageCreateInfo     m_imageInfo{};
    VkImageViewCreateInfo m_imageViewInfo{};
    VkDescriptorImageInfo m_descriptorInfo{};

    // For Texture
    VkSampler m_sampler = VK_NULL_HANDLE;
    
    uint32_t MaxMipLevel(int width, int height);
    static unsigned int GetChannelSize(VkFormat format);
    static VkImageCreateInfo     GetDefaultImageCreateInfo();
    static VkImageViewCreateInfo GetDefaultImageViewCreateInfo();
    static VkImageMemoryBarrier  GetDefaultImageMemoryBarrier();
    
    void cmdCall(void (Image::*cmdFunc)(VkCommandBuffer));
    
};
