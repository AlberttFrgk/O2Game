/*
 * MIT License
 *
 * Copyright (c) 2023 Estrol Mendex
 * See the LICENSE file in the root of this project for details.
 */

#include "VkDevice.h"
#include "vkinit.h"
#include <Exceptions/EstException.h>
#include <Graphics/NativeWindow.h>
#include <SDL2/SDL_vulkan.h>
#include <Logs.h>

VK_Device::VK_Device(
    std::string appName,
    std::string engineName)
{
    if (volkInitialize() != VK_SUCCESS) {
        throw Exceptions::EstException("Failed to initialize Vulkan");
    }

    vkb::InstanceBuilder instance_builder;
    auto                 instance_builder_return = instance_builder
                                       .set_app_name(appName.c_str())
                                       .set_engine_name(engineName.c_str())
                                       .request_validation_layers(true)
                                       .require_api_version(1, 1, 0)
                                       .build();

    if (!instance_builder_return) {
        throw Exceptions::EstException("Failed to create Vulkan instance");
    }

    vkb::Instance vkb_instance = instance_builder_return.value();
    volkLoadInstance(vkb_instance.instance);

    auto windowHandle = Graphics::NativeWindow::Get()->GetWindow();

    if (!SDL_Vulkan_CreateSurface(
            windowHandle,
            vkb_instance.instance,
            &surface)) {
        throw Exceptions::EstException("Failed to create Vulkan surface");
    }

    vkb::PhysicalDeviceSelector selector{ vkb_instance };
    vkb::PhysicalDevice         physical_device = selector
                                              .set_minimum_version(1, 1)
                                              .set_surface(surface)
                                              .select()
                                              .value();

    vkb::DeviceBuilder device_builder{ physical_device };
    vkb::Device        vkb_device = device_builder.build().value();

    // Get the GPU device's name
    std::string deviceName = physical_device.name;

    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(physical_device.physical_device, &deviceProperties);

    auto queue = vkb_device.get_queue(vkb::QueueType::graphics).value();
    auto queueFamily = vkb_device.get_queue_index(vkb::QueueType::graphics).value();

    graphicsQueue = queue;
    graphicsQueueFamily = queueFamily;
    vkbInstance = vkb_instance;
    vkbDevice = vkb_device;
    MSAASampleCount = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT;

    // Logs::Debug("Vulkan device created: %s", deviceName.c_str());
}

VK_Device::~VK_Device()
{
    if (vkbDevice) {
        vkDeviceWaitIdle(vkbDevice.device);

        vkDestroySurfaceKHR(vkbInstance.instance, surface, nullptr);
        vkb::destroy_device(vkbDevice);
        vkb::destroy_instance(vkbInstance);
    }
}