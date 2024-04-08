/*
 * MIT License
 *
 * Copyright (c) 2023 Estrol Mendex
 * See the LICENSE file in the root of this project for details.
 */

#ifndef __RENDERPASS_H_
#define __RENDERPASS_H_

#include "VkDevice.h"

class VK_Renderpass
{
public:
    VK_Renderpass() = default;
    VK_Renderpass(
        VkRenderPassCreateInfo  &createInfo,
        VkFramebufferCreateInfo &framebufferCreateInfo,
        VK_Device               *device,
        std::vector<VkImageView> attachments,
        VkImageView              depthAttachment,
        uint32_t                 imageCount);

    ~VK_Renderpass();

    VkClearValue getClearValue();
    void         setClearValue(VkClearValue clearValue);

    VkClearValue getDepthClearValue();
    void         setDepthClearValue(VkClearValue clearValue);

    VkExtent2D getExtent();
    void       setExtent(VkExtent2D extent);

    void Begin(VkCommandBuffer cmd, uint32_t imageIndex);
    void End(VkCommandBuffer cmd);

    VkRenderPass getRenderpass()
    {
        return renderpass;
    }

    VkFramebuffer getFramebuffer(uint32_t index)
    {
        return framebuffers[index];
    }

private:
    VkRenderPass               renderpass;
    std::vector<VkFramebuffer> framebuffers;
    VkClearValue               clearValues[2];
    VkExtent2D                 extent;

    VK_Device *device = nullptr;
};

#endif