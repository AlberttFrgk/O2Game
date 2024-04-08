/*
 * MIT License
 *
 * Copyright (c) 2023 Estrol Mendex
 * See the LICENSE file in the root of this project for details.
 */

#include "VkRenderchain.h"

VK_Renderchain::VK_Renderchain()
{
}

VK_Renderchain::VK_Renderchain(VK_Device *device)
{
    _device = device;
}

VK_Renderchain::~VK_Renderchain()
{
    destroyRenderchain();
}

void VK_Renderchain::createRenderchain(
    VkExtent2D extent,
    VkFormat   format,
    VkFormat   depthFormat,
    uint32_t   imageCount)
{
    images.resize(imageCount);

    for (uint32_t i = 0; i < imageCount; i++) {
        images[i] = std::make_shared<VK_Image>(
            _device,
            format,
            extent,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            VK_SAMPLE_COUNT_1_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT);
    }

    depthImage = std::make_shared<VK_Image>(
        _device,
        depthFormat,
        extent,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_SAMPLE_COUNT_1_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT);

    this->extent = extent;
}

void VK_Renderchain::destroyRenderchain()
{
    if (_device) {
        depthImage.reset();

        for (auto &image : images) {
            image.reset();
        }

        images.clear();
    }
}