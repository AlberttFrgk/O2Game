/*
 * MIT License
 *
 * Copyright (c) 2023 Estrol Mendex
 * See the LICENSE file in the root of this project for details.
 */

#ifndef __RENDERCHAIN_H_
#define __RENDERCHAIN_H_

#include "VkDevice.h"
#include "VkImage.h"

#include <memory>

class VK_Renderchain
{
public:
    VK_Renderchain();
    VK_Renderchain(VK_Device *device);
    ~VK_Renderchain();

    void createRenderchain(
        VkExtent2D extent,
        VkFormat   format,
        VkFormat   depthFormat,
        uint32_t   imageCount);

    void destroyRenderchain();

    std::vector<std::shared_ptr<VK_Image>> &getImages()
    {
        return images;
    }

    std::shared_ptr<VK_Image> &getDepthImage()
    {
        return depthImage;
    }

    VkExtent2D getExtent()
    {
        return extent;
    }

private:
    VK_Device                             *_device = nullptr;
    std::vector<std::shared_ptr<VK_Image>> images;
    std::shared_ptr<VK_Image>              depthImage;
    VkExtent2D                             extent;
};

#endif