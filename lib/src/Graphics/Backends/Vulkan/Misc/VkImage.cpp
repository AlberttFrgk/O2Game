/*
 * MIT License
 *
 * Copyright (c) 2023 Estrol Mendex
 * See the LICENSE file in the root of this project for details.
 */

#include "VkImage.h"
#include "vkinit.h"

#include <Exceptions/EstException.h>

#include <Logs.h>
static int g_DbgImage = 0;

VK_Image::VK_Image(
    VK_Device            *device,
    VkFormat              format,
    VkExtent2D            extent,
    VkImageUsageFlags     usage,
    VkSampleCountFlagBits sampleCount,
    VkMemoryPropertyFlags properties,
    VkImageAspectFlagBits aspect)
{
    _device = device;
    auto vkdevice = device->getDevice();

    VkExtent3D imageExtent = {
        .width = extent.width,
        .height = extent.height,
        .depth = 1
    };

    VkImageCreateInfo dimg_info = vkinit::image_create_info(
        format,
        sampleCount,
        usage,
        imageExtent);

    auto result = vkCreateImage(vkdevice, &dimg_info, nullptr, &image);
    if (result != VK_SUCCESS) {
        throw Exceptions::EstException("Failed to create image");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(vkdevice, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = vkinit::find_memory_type(
        device->getGPUDevice(),
        memRequirements.memoryTypeBits,
        properties);

    result = vkAllocateMemory(vkdevice, &allocInfo, nullptr, &memory);
    if (result != VK_SUCCESS) {
        throw Exceptions::EstException("Failed to allocate image memory");
    }

    vkBindImageMemory(vkdevice, image, memory, 0);

    VkImageViewCreateInfo viewInfo = vkinit::imageview_create_info(format, image, aspect);
    result = vkCreateImageView(vkdevice, &viewInfo, nullptr, &view);

    if (result != VK_SUCCESS) {
        throw Exceptions::EstException("Failed to create image view");
    }

    this->format = format;
    this->extent = extent;
    this->sampleCount = sampleCount;
    this->layout = VK_IMAGE_LAYOUT_UNDEFINED;

    g_DbgImageId = g_DbgImage++;
    Logs::Debug("Created image %d", g_DbgImageId);
}

VK_Image::~VK_Image()
{
    Logs::Debug("Destroying image %d", g_DbgImageId);

    if (_device) {
        auto device = _device->getDevice();

        vkDestroyImageView(device, view, nullptr);
        vkDestroyImage(device, image, nullptr);
        vkFreeMemory(device, memory, nullptr);
    }
}