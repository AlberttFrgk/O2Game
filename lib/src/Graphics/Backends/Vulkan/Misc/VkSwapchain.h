/*
 * MIT License
 *
 * Copyright (c) 2023 Estrol Mendex
 * See the LICENSE file in the root of this project for details.
 */

#ifndef __SWAPCHAIN_H_
#define __SWAPCHAIN_H_

#include "VkDevice.h"
#include "VkImage.h"
#include <memory>

#include "../third-party/Volk/volk.h"
#include "../third-party/VulkanBootstrap/VkBootstrap.h"

class VK_Swapchain
{
public:
    VK_Swapchain() = default;
    VK_Swapchain(VK_Device *device);
    ~VK_Swapchain();

    bool createSwapchain(
        bool       vsync,
        VkExtent2D extent);

    void destroySwapchain();

    void accquireNextImage(uint32_t &imageIndex, VkSemaphore semaphore, VkFence fence);
    void presentImage(VkPresentInfoKHR &presentInfo);

    VkSwapchainKHR getSwapchain()
    {
        return swapchain.swapchain;
    }

    vkb::Swapchain getVkbSwapchain()
    {
        return swapchain;
    }

    VkExtent2D getExtent()
    {
        return swapchain.extent;
    }

    VkFormat getFormat()
    {
        return swapchain.image_format;
    }

    VkFormat getDepthFormat()
    {
        return depthImage->getFormat();
    }

    uint32_t getImageCount()
    {
        return swapchain.image_count;
    }

    bool isVsync()
    {
        return vsync;
    }

    bool isReady()
    {
        return swapchainReady;
    }

    std::vector<VkImage> &getImages()
    {
        return swapchainImagesVk;
    }

    std::vector<VkImageView> &getImageViews()
    {
        return swapchainImageViews;
    }

    std::shared_ptr<VK_Image> &getDepthImage()
    {
        return depthImage;
    }

private:
    vkb::Swapchain swapchain;

    std::vector<VkImage>      swapchainImagesVk;
    std::vector<VkImageView>  swapchainImageViews;
    std::shared_ptr<VK_Image> depthImage;

    VK_Device *device = nullptr;
    bool       swapchainReady;
    bool       vsync;
};

#endif // __SWAPCHAIN_H_