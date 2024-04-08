/*
 * MIT License
 *
 * Copyright (c) 2023 Estrol Mendex
 * See the LICENSE file in the root of this project for details.
 */

#include "VkSwapchain.h"
#include "vkinit.h"

#include <Exceptions/EstException.h>
#include <Graphics/NativeWindow.h>

VK_Swapchain::VK_Swapchain(VK_Device *device)
{
    this->device = device;
    swapchain.swapchain = VK_NULL_HANDLE;
}

bool VK_Swapchain::createSwapchain(
    bool       vsync,
    VkExtent2D extent)
{
    auto     rect = Graphics::NativeWindow::Get()->GetWindowSize();
    uint32_t width = (uint32_t)rect.Width;
    uint32_t height = (uint32_t)rect.Height;

    VkPresentModeKHR preset = VK_PRESENT_MODE_FIFO_KHR;
    if (!vsync) {
        preset = VK_PRESENT_MODE_IMMEDIATE_KHR;
    }

    VkSurfaceFormatKHR surfaceFormat = {
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
    };

    VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    auto vkDevice = device->getVkbDevice();
    auto vkSurface = device->getSurface();

    auto builder = vkb::SwapchainBuilder(vkDevice, vkSurface);
    builder.set_desired_format(surfaceFormat)
        .set_desired_extent(width, height)
        .set_image_usage_flags(usage)
        .set_desired_present_mode(preset);

    builder.set_old_swapchain(swapchain);

    auto resultbuild = builder.build();
    if (!resultbuild) {
        if (resultbuild.error() == vkb::SwapchainError::invalid_window_size) {
            destroySwapchain();
        }

        swapchain.swapchain = VK_NULL_HANDLE;
        return false;
    }

    destroySwapchain();

    swapchain = resultbuild.value();

    auto vkimages = swapchain.get_images();
    if (!vkimages) {
        vkb::destroy_swapchain(swapchain);
        throw Exceptions::EstException("Failed to get swapchain images");
    }

    auto vkbviews = swapchain.get_image_views();
    if (!vkbviews) {
        vkb::destroy_swapchain(swapchain);
        throw Exceptions::EstException("Failed to get swapchain image views");
    }

    swapchainImagesVk = vkimages.value();
    swapchainImageViews = vkbviews.value();

    rect = Graphics::NativeWindow::Get()->GetBufferSize();
    VkExtent2D renderSize = {
        .width = (uint32_t)rect.Width,
        .height = (uint32_t)rect.Height
    };

    // create depth image for both render and present

    VkFormat depthFormat = vkinit::get_supported_depth_format(device->getGPUDevice());
    rect = Graphics::NativeWindow::Get()->GetWindowSize();
    VkExtent2D newSize = {
        .width = (uint32_t)rect.Width,
        .height = (uint32_t)rect.Height
    };

    auto depthImagePresent = std::make_shared<VK_Image>(
        device,
        depthFormat,
        newSize,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        device->getMSAASampleCount(),
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT);

    depthImage = depthImagePresent;

    this->swapchainReady = true;
    return true;
}

void VK_Swapchain::destroySwapchain()
{
    swapchainImagesVk.clear();
    swapchainImageViews.clear();

    vkb::destroy_swapchain(swapchain);
}

VK_Swapchain::~VK_Swapchain()
{
    destroySwapchain();
}

void VK_Swapchain::accquireNextImage(uint32_t &imageIndex, VkSemaphore semaphore, VkFence fence)
{
    if (!this->swapchainReady) {
        return;
    }

    VkResult result = vkAcquireNextImageKHR(
        this->device->getDevice(),
        this->swapchain.swapchain,
        UINT64_MAX,
        semaphore,
        fence,
        &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        this->swapchainReady = false;
    } else {
        if (result != VK_SUCCESS) {
            throw Exceptions::EstException("Failed to acquire next image");
        }
    }
}

void VK_Swapchain::presentImage(VkPresentInfoKHR &presentInfo)
{
    if (!this->swapchainReady) {
        return;
    }

    VkResult result = vkQueuePresentKHR(
        this->device->getGraphicsQueue(),
        &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        this->swapchainReady = false;
    } else {
        if (result != VK_SUCCESS) {
            throw Exceptions::EstException("Failed to present image");
        }
    }
}