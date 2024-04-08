/*
 * MIT License
 *
 * Copyright (c) 2023 Estrol Mendex
 * See the LICENSE file in the root of this project for details.
 */

#ifndef __MEMORY_H_
#define __MEMORY_H_

#include "VkDevice.h"

class VK_Memory
{
public:
    VK_Memory(
        VK_Device            *device,
        VkDeviceSize          size,
        VkBufferUsageFlags    memoryTypeBits,
        VkMemoryPropertyFlags properties,
        VkSharingMode         sharingMode = VK_SHARING_MODE_EXCLUSIVE);
    ~VK_Memory();

    void *Data();

    VkBuffer getBuffer()
    {
        return buffer;
    }

    VkDeviceMemory getMemory()
    {
        return memory;
    }

private:
    VkBuffer       buffer;
    VkDeviceMemory memory;

    VK_Device *device = nullptr;
    void      *data = nullptr;

    int g_DbgMemoryId = 0;
};

#endif