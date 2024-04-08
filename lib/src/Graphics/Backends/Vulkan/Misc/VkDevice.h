/*
 * MIT License
 *
 * Copyright (c) 2023 Estrol Mendex
 * See the LICENSE file in the root of this project for details.
 */

#ifndef __DEVICE_H_
#define __DEVICE_H_

#include "../third-party/VulkanBootstrap/VkBootstrap.h"
#include <string>

class VK_Device
{
public:
    VK_Device(std::string appName = "SampleAppName", std::string engineName = "EstEngine");
    ~VK_Device();

    VkDevice    getDevice() { return vkbDevice.device; }
    vkb::Device getVkbDevice() { return vkbDevice; }

    VkQueue               getGraphicsQueue() { return graphicsQueue; }
    uint32_t              getGraphicsQueueFamily() { return graphicsQueueFamily; }
    VkSampleCountFlagBits getMSAASampleCount() { return MSAASampleCount; }
    VkSurfaceKHR          getSurface() { return surface; }
    VkPhysicalDevice      getGPUDevice() { return vkbDevice.physical_device; }
    VkInstance            getInstance() { return vkbInstance.instance; }

private:
    vkb::Instance vkbInstance;
    vkb::Device   vkbDevice;
    VkSurfaceKHR  surface;

    VkQueue               graphicsQueue;
    uint32_t              graphicsQueueFamily;
    VkSampleCountFlagBits MSAASampleCount;
};

#endif // __DEVICE_H_