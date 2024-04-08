/*
 * MIT License
 *
 * Copyright (c) 2023 Estrol Mendex
 * See the LICENSE file in the root of this project for details.
 */

#include "VulkanTexture2D.h"
#include "./Misc/vkinit.h"
#include "VulkanBackend.h"
#include "VulkanDescriptor.h"
#include <Exceptions/EstException.h>
#include <Misc/Filesystem.h>
#include <fstream>

#include <Graphics/Renderer.h>
#include <Graphics/Utils/stb_image.h>

#include "../../ImguiBackends/imgui_impl_vulkan.h"

using namespace Graphics;
using namespace Exceptions;

VKTexture2D::VKTexture2D(TextureSamplerInfo samplerInfo)
{
    Descriptor = nullptr;
    SamplerInfo = samplerInfo;
}

VKTexture2D::~VKTexture2D()
{
    if (Descriptor) {
        auto renderer = Graphics::Renderer::Get();
        if (renderer->GetAPI() == Graphics::API::Vulkan) {
            auto vulkan = (Graphics::Backends::Vulkan *)renderer->GetBackend();
            vulkan->DestroyDescriptor(Descriptor);
        }
    }
}

void VKTexture2D::Load(std::filesystem::path path)
{
    if (Descriptor) {
        throw EstException("Cannot initialize texture twice");
    }

    auto data = Misc::Filesystem::ReadFile(path);

    Load((const unsigned char *)data.data(), data.size());

    RefName = path.string();
    Descriptor->RefName = RefName;
}

void VKTexture2D::Load(const unsigned char *buf, size_t size)
{
    if (Descriptor) {
        throw EstException("Cannot initialize texture twice");
    }

    auto renderer = Graphics::Renderer::Get();
    if (renderer->GetAPI() != Graphics::API::Vulkan) {
        throw EstException("Cannot load Vulkan texture from non-Vulkan renderer");
    }

    auto vulkan = (Graphics::Backends::Vulkan *)renderer->GetBackend();
    Descriptor = vulkan->CreateDescriptor();
    unsigned char *image_data = stbi_load_from_memory(
        (const unsigned char *)buf,
        (int)size,
        &Descriptor->Size.Width,
        &Descriptor->Size.Height,
        &Descriptor->Channels,
        STBI_rgb_alpha);

    if (!image_data) {
        throw EstException("Failed to load image");
    }

    Descriptor->Channels = 4; // always use RGBA

    Load((const unsigned char *)image_data, (uint32_t)Descriptor->Size.Width, (uint32_t)Descriptor->Size.Height);
    stbi_image_free(image_data);
}

uint32_t powerOfTwoSize(uint32_t size)
{
    uint32_t power = 1;
    while (power < size) {
        power *= 2;
    }
    return power;
}

void VKTexture2D::Load(const unsigned char *pixbuf, uint32_t width, uint32_t height)
{
    auto renderer = Graphics::Renderer::Get();
    if (renderer->GetAPI() != Graphics::API::Vulkan) {
        throw EstException("Cannot load Vulkan texture from non-Vulkan renderer");
    }

    RefName = "Texture2D_Internal";

    auto vulkan = (Graphics::Backends::Vulkan *)renderer->GetBackend();
    auto vkobject = vulkan->GetDevice();

    if (!Descriptor) {
        Descriptor = vulkan->CreateDescriptor();
        Descriptor->Channels = 4;
    }

    Descriptor->Size = {
        0, 0,
        (int)width, (int)height
    };

    Descriptor->RefName = RefName;

    size_t image_size = static_cast<size_t>(Descriptor->Size.Width) * static_cast<size_t>(Descriptor->Size.Height) * Descriptor->Channels;

    VkExtent2D imageSize = {
        .width = (uint32_t)width,
        .height = (uint32_t)height
    };

    std::shared_ptr<VK_Image> image = std::make_shared<VK_Image>(
        vkobject,
        VK_FORMAT_R8G8B8A8_UNORM,
        imageSize,
        (VkImageUsageFlagBits)(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT),
        VK_SAMPLE_COUNT_1_BIT);

    Descriptor->ImageData = image;
    Descriptor->Sampler = vulkan->CreateSampler(SamplerInfo);

    VkResult err;
    {
        auto descriptorPool = vulkan->GetDescriptorPool();

        VkDescriptorSet descriptor_set;
        {
            VkDescriptorSetAllocateInfo alloc_info = {};
            alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            alloc_info.descriptorPool = descriptorPool->pool;
            alloc_info.descriptorSetCount = 1;
            alloc_info.pSetLayouts = &descriptorPool->layout;
            err = vkAllocateDescriptorSets(vkobject->getDevice(), &alloc_info, &descriptor_set);

            if (err != VK_SUCCESS) {
                throw EstException("Failed to allocate vulkan descriptor set");
            }
        }
        {
            VkDescriptorImageInfo desc_image[1] = {};
            desc_image[0].sampler = Descriptor->Sampler;
            desc_image[0].imageView = image->getView();
            desc_image[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            VkWriteDescriptorSet write_desc[1] = {};
            write_desc[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write_desc[0].dstSet = descriptor_set;
            write_desc[0].descriptorCount = 1;
            write_desc[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write_desc[0].pImageInfo = desc_image;
            vkUpdateDescriptorSets(vkobject->getDevice(), 1, write_desc, 0, nullptr);
        }

        Descriptor->VkId = descriptor_set;
    }

    {
        VkDescriptorImageInfo desc_image[1] = {};
        desc_image[0].sampler = Descriptor->Sampler;
        desc_image[0].imageView = Descriptor->ImageData->getView();
        desc_image[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet write_desc[1] = {};
        write_desc[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write_desc[0].dstSet = Descriptor->VkId;
        write_desc[0].descriptorCount = 1;
        write_desc[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write_desc[0].pImageInfo = desc_image;

        vkUpdateDescriptorSets(vkobject->getDevice(), 1, write_desc, 0, nullptr);
    }

    std::shared_ptr<VK_Memory> memory = std::make_shared<VK_Memory>(
        vkobject,
        (VkDeviceSize)image_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    Descriptor->UploadBuffer = memory;

    {
        void *map = Descriptor->UploadBuffer->Data();
        memcpy(map, pixbuf, image_size);

        VkMappedMemoryRange range[1] = {};
        range[0].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        range[0].memory = Descriptor->UploadBuffer->getMemory();
        range[0].size = image_size;

        err = vkFlushMappedMemoryRanges(vkobject->getDevice(), 1, range);
        if (err != VK_SUCCESS) {
            throw EstException("Vulkan: Failed to flush mapped memory range");
        }
    }

    // if (Descriptor->Size.Width == 213 && Descriptor->Size.Height == 125) {
    //     __debugbreak();
    // }

    vulkan->ImmediateSubmit([=](VkCommandBuffer cmd) {
        VkImageMemoryBarrier copy_barrier[1] = {};
        copy_barrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        copy_barrier[0].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        copy_barrier[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        copy_barrier[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        copy_barrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        copy_barrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        copy_barrier[0].image = Descriptor->ImageData->getImage();
        copy_barrier[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy_barrier[0].subresourceRange.levelCount = 1;
        copy_barrier[0].subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, copy_barrier);

        VkBufferImageCopy region = {};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent.width = Descriptor->Size.Width;
        region.imageExtent.height = Descriptor->Size.Height;
        region.imageExtent.depth = 1;
        vkCmdCopyBufferToImage(
            cmd,
            Descriptor->UploadBuffer->getBuffer(),
            Descriptor->ImageData->getImage(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        VkImageMemoryBarrier use_barrier[1] = {};
        use_barrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        use_barrier[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        use_barrier[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        use_barrier[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        use_barrier[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        use_barrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        use_barrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        use_barrier[0].image = Descriptor->ImageData->getImage();
        use_barrier[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        use_barrier[0].subresourceRange.levelCount = 1;
        use_barrier[0].subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, use_barrier);
    });
}

const void *VKTexture2D::GetId()
{
    return Descriptor->VkId;
}

const Rect VKTexture2D::GetSize()
{
    return Descriptor->Size;
}
