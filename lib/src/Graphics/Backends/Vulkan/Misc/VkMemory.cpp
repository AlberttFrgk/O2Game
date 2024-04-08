/*
 * MIT License
 *
 * Copyright (c) 2023 Estrol Mendex
 * See the LICENSE file in the root of this project for details.
 */

#include "VkMemory.h"
#include "vkinit.h"

#include <Logs.h>
static int g_DbgMemory = 0;

VK_Memory::VK_Memory(
    VK_Device            *device,
    VkDeviceSize          size,
    VkBufferUsageFlags    memoryTypeBits,
    VkMemoryPropertyFlags properties,
    VkSharingMode         sharingMode)
{
    this->device = device;

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = memoryTypeBits;
    bufferInfo.sharingMode = sharingMode;

    if (vkCreateBuffer(device->getDevice(), &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device->getDevice(), buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = vkinit::find_memory_type(
        device->getGPUDevice(),
        memRequirements.memoryTypeBits,
        properties);

    if (vkAllocateMemory(device->getDevice(), &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate buffer memory");
    }

    vkBindBufferMemory(device->getDevice(), buffer, memory, 0);
    vkMapMemory(device->getDevice(), memory, 0, size, 0, &data);

    g_DbgMemoryId = g_DbgMemory++;
    // Logs::Debug("Created memory %d", g_DbgMemoryId);
}

VK_Memory::~VK_Memory()
{
    // Logs::Debug("Destroying memory %d", g_DbgMemoryId);

    if (device) {
        vkUnmapMemory(device->getDevice(), memory);
        vkDestroyBuffer(device->getDevice(), buffer, nullptr);
        vkFreeMemory(device->getDevice(), memory, nullptr);
    }
}

void *VK_Memory::Data()
{
    return data;
}