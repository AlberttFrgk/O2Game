/*
 * MIT License
 *
 * Copyright (c) 2023 Estrol Mendex
 * See the LICENSE file in the root of this project for details.
 */

#ifndef __IMAGE_H_
#define __IMAGE_H_

#include "VkDevice.h"

#include "../third-party/Volk/volk.h"

class VK_Image
{
public:
    VK_Image() = default;
    VK_Image(
        VK_Device            *device,
        VkFormat              format,
        VkExtent2D            extent,
        VkImageUsageFlags     usage,
        VkSampleCountFlagBits sampleCount,
        VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VkImageAspectFlagBits aspect = VK_IMAGE_ASPECT_COLOR_BIT);

    ~VK_Image();

    VkImage               getImage() { return image; }
    VkDeviceMemory        getMemory() { return memory; }
    VkImageView           getView() { return view; }
    VkFormat              getFormat() { return format; }
    VkImageLayout         getLayout() { return layout; }
    VkExtent2D            getExtent() { return extent; }
    VkSampleCountFlagBits getSampleCount() { return sampleCount; }
    int                   getId() { return g_DbgImageId; }

private:
    VK_Device *_device = nullptr;

    VkImage               image;
    VkDeviceMemory        memory;
    VkImageView           view;
    VkFormat              format;
    VkExtent2D            extent;
    VkSampleCountFlagBits sampleCount;
    VkImageLayout         layout;

    int g_DbgImageId = 0;
};

#endif