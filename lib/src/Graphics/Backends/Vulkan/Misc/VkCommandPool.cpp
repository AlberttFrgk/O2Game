/*
 * MIT License
 *
 * Copyright (c) 2023 Estrol Mendex
 * See the LICENSE file in the root of this project for details.
 */

#include "VkCommandPool.h"
#include <Exceptions/EstException.h>

#include <Logs.h>
static int g_DbgCmdPool = 0;

VK_CommandPool::VK_CommandPool(VK_Device *device, uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags)
{
    this->vkdevice = device;

    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndex;
    poolInfo.flags = flags;

    if (vkCreateCommandPool(device->getDevice(), &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw Exceptions::EstException("Failed to create command pool");
    }

    g_DbgCmdPoolId = g_DbgCmdPool++;
    Logs::Debug("Created command pool %d", g_DbgCmdPoolId);
}

VK_CommandPool::~VK_CommandPool()
{
    Logs::Debug("Destroying command pool %d", g_DbgCmdPoolId);

    if (vkdevice) {
        vkDestroyCommandPool(vkdevice->getDevice(), commandPool, nullptr);
    }
}

VkCommandBuffer VK_CommandPool::allocateCommandBuffer(VkCommandBufferLevel level, bool begin)
{
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = level;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(vkdevice->getDevice(), &allocInfo, &commandBuffer);

    if (begin) {
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);
    }

    return commandBuffer;
}

void VK_CommandPool::freeCommandBuffer(VkCommandBuffer cmd, bool end)
{
    if (end) {
        vkEndCommandBuffer(cmd);
    }

    vkFreeCommandBuffers(vkdevice->getDevice(), commandPool, 1, &cmd);
}

VkCommandPool VK_CommandPool::getCommandPool()
{
    return commandPool;
}