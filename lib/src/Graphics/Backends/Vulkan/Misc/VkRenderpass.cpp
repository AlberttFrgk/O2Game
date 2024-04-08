/*
 * MIT License
 *
 * Copyright (c) 2023 Estrol Mendex
 * See the LICENSE file in the root of this project for details.
 */

#include "VkRenderpass.h"
#include "vkinit.h"

#include <Exceptions/EstException.h>

VK_Renderpass::VK_Renderpass(
    VkRenderPassCreateInfo  &createInfo,
    VkFramebufferCreateInfo &framebufferCreateInfo,
    VK_Device               *device,
    std::vector<VkImageView> attachments,
    VkImageView              depthAttachment,
    uint32_t                 imageCount)
{
    this->device = device;

    auto result = vkCreateRenderPass(device->getDevice(), &createInfo, nullptr, &renderpass);
    if (result != VK_SUCCESS) {
        throw Exceptions::EstException("Failed to create renderpass");
    }

    framebufferCreateInfo.renderPass = renderpass;
    framebuffers.resize(imageCount);

    for (uint32_t i = 0; i < imageCount; i++) {
        VkImageView attachmentsBody[2];
        attachmentsBody[0] = attachments[i];
        attachmentsBody[1] = depthAttachment;

        framebufferCreateInfo.pAttachments = attachmentsBody;
        framebufferCreateInfo.attachmentCount = 2;

        result = vkCreateFramebuffer(device->getDevice(), &framebufferCreateInfo, nullptr, &framebuffers[i]);
        if (result != VK_SUCCESS) {
            throw Exceptions::EstException("Failed to create framebuffer");
        }
    }

    clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
    clearValues[1].depthStencil = { 1.0f, 0 };

    extent = {
        .width = framebufferCreateInfo.width,
        .height = framebufferCreateInfo.height
    };
}

VK_Renderpass::~VK_Renderpass()
{
    for (auto framebuffer : framebuffers) {
        vkDestroyFramebuffer(device->getDevice(), framebuffer, nullptr);
    }

    vkDestroyRenderPass(device->getDevice(), renderpass, nullptr);
}

VkClearValue VK_Renderpass::getClearValue()
{
    return clearValues[0];
}

void VK_Renderpass::setClearValue(VkClearValue clearValue)
{
    clearValues[0] = clearValue;
}

VkClearValue VK_Renderpass::getDepthClearValue()
{
    return clearValues[1];
}

void VK_Renderpass::setDepthClearValue(VkClearValue clearValue)
{
    clearValues[1] = clearValue;
}

VkExtent2D VK_Renderpass::getExtent()
{
    return extent;
}

void VK_Renderpass::setExtent(VkExtent2D extent)
{
    this->extent = extent;
}

void VK_Renderpass::Begin(VkCommandBuffer cmd, uint32_t imageIndex)
{
    VkRenderPassBeginInfo rpInfo = vkinit::renderpass_begin_info(
        renderpass,
        extent,
        framebuffers[imageIndex]);

    rpInfo.clearValueCount = 2;
    rpInfo.pClearValues = clearValues;

    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void VK_Renderpass::End(VkCommandBuffer cmd)
{
    vkCmdEndRenderPass(cmd);
}