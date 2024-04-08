/*
 * MIT License
 *
 * Copyright (c) 2023 Estrol Mendex
 * See the LICENSE file in the root of this project for details.
 */

#ifndef __COMMANDPOOL_H_
#define __COMMANDPOOL_H_

#include "VkDevice.h"

class VK_CommandPool
{
public:
    VK_CommandPool() = default;
    VK_CommandPool(VK_Device *device, uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0);
    ~VK_CommandPool();

    VkCommandBuffer allocateCommandBuffer(
        VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        bool                 begin = false);

    void freeCommandBuffer(
        VkCommandBuffer cmd,
        bool            end = false);

    VkCommandPool getCommandPool();

private:
    VK_Device    *vkdevice = nullptr;
    VkCommandPool commandPool;

    int g_DbgCmdPoolId = 0;
};

#endif // __COMMANDPOOL_H_