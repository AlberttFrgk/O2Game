#include "Rendering/Vulkan/Texture2DVulkan.h"
#include "../../Data/Imgui/imgui_impl_vulkan.h"
#include "Rendering/Vulkan/VulkanEngine.h"
#include "Rendering/WindowsTypes.h"
#include "Texture/ImageGenerator.h"
#include "Texture2DVulkan_Internal.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../../Data/stb_image.h"
#include <fstream>
#include <iostream>
#include <mutex>
#include <vector>

static std::unordered_map<int, std::unique_ptr<Texture2D_Vulkan>> m_textures;
static int                                                        m_textureCount = 0;
static std::mutex                                                 m_textureMutex;
static std::unique_ptr<Texture2D_Vulkan>                          m_dummyTexture;

Texture2D_Vulkan* CreateTexture()
{
    // find the empty slot
    for (int i = 0; i < m_textureCount; i++) {
        if (m_textures[i] == nullptr) {
            m_textures[i] = std::make_unique<Texture2D_Vulkan>();
            m_textures[i]->Id = i;
            return m_textures[i].get();
        }
    }

    // if no empty slot, create a new one
    m_textures[m_textureCount] = std::make_unique<Texture2D_Vulkan>();
    m_textures[m_textureCount]->Id = m_textureCount;
    m_textureCount++;

    return m_textures[m_textureCount - 1].get();
}

uint32_t findMemoryType(VkPhysicalDevice phyiscalDevice, uint32_t type_filter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(phyiscalDevice, &mem_properties);

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++)
        if ((type_filter & (1 << i)) && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties)
            return i;

    return 0xFFFFFFFF; // Unable to find memoryType
}

uint32_t vkTexture::FindMemoryType(uint32_t type_filter, uint32_t properties)
{
    return findMemoryType(VulkanEngine::GetInstance()->_chosenGPU, type_filter, properties);
}

Texture2D_Vulkan* vkTexture::TexLoadImage(std::filesystem::path imagePath)
{
    std::fstream fs(imagePath, std::ios::binary | std::ios::in);
    if (!fs.is_open()) {
        return nullptr;
    }

    fs.seekg(0, std::ios::end);
    size_t size = fs.tellg();
    fs.seekg(0, std::ios::beg);

    std::vector<char> buffer(size);
    fs.read(buffer.data(), size);
    fs.close();

    return TexLoadImage(buffer.data(), size);
}

void InternalLoad(
    VulkanEngine     *vulkan_driver,
    Texture2D_Vulkan *tex_data,
    unsigned char    *image_data)
{

    size_t image_size = static_cast<size_t>(tex_data->Width) * static_cast<size_t>(tex_data->Height) * tex_data->Channels;

    // Premultiply alpha
    for (size_t i = 0; i < image_size; i += tex_data->Channels) { // Fix white line issue
        float alpha = image_data[i + 3] / 255.0f;
        image_data[i] = static_cast<unsigned char>(image_data[i] * alpha);
        image_data[i + 1] = static_cast<unsigned char>(image_data[i + 1] * alpha);
        image_data[i + 2] = static_cast<unsigned char>(image_data[i + 2] * alpha);
    }

    VkResult err;
    {
        VkImageCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        info.imageType = VK_IMAGE_TYPE_2D;
        info.format = VK_FORMAT_R8G8B8A8_UNORM;
        info.extent.width = tex_data->Width;
        info.extent.height = tex_data->Height;
        info.extent.depth = 1;
        info.mipLevels = 1;
        info.arrayLayers = 1;
        info.samples = VK_SAMPLE_COUNT_1_BIT;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        err = vkCreateImage(vulkan_driver->_device, &info, nullptr, &tex_data->Image);
        if (err != VK_SUCCESS) {
            throw std::runtime_error("Vulkan: Failed to create image");
        }

        VkMemoryRequirements req;
        vkGetImageMemoryRequirements(vulkan_driver->_device, tex_data->Image, &req);
        VkMemoryAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = req.size;
        alloc_info.memoryTypeIndex = findMemoryType(vulkan_driver->_chosenGPU, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        err = vkAllocateMemory(vulkan_driver->_device, &alloc_info, nullptr, &tex_data->ImageMemory);
        if (err != VK_SUCCESS) {
            throw std::runtime_error("Vulkan: Failed to allocate image memory");
        }

        err = vkBindImageMemory(vulkan_driver->_device, tex_data->Image, tex_data->ImageMemory, 0);
        if (err != VK_SUCCESS) {
            throw std::runtime_error("Vulkan: Failed to bind image memory");
        }
    }

    {
        VkImageViewCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.image = tex_data->Image;
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = VK_FORMAT_R8G8B8A8_UNORM;
        info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        info.subresourceRange.levelCount = 1;
        info.subresourceRange.layerCount = 1;
        err = vkCreateImageView(vulkan_driver->_device, &info, nullptr, &tex_data->ImageView);
        if (err != VK_SUCCESS) {
            throw std::runtime_error("Vulkan: Failed to create image view");
        }
    }

    {
        VkSamplerCreateInfo samplerInfo = {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 1.0f;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
        err = vkCreateSampler(vulkan_driver->_device, &samplerInfo, nullptr, &tex_data->Sampler);
        if (err != VK_SUCCESS) {
            throw std::runtime_error("Vulkan: Failed to create image sampler");
        }
    }

    tex_data->DS = ImGui_ImplVulkan_AddTexture(tex_data->Sampler, tex_data->ImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    {
        VkBufferCreateInfo buffer_info = {};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = image_size;
        buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        err = vkCreateBuffer(vulkan_driver->_device, &buffer_info, nullptr, &tex_data->UploadBuffer);
        if (err != VK_SUCCESS) {
            throw std::runtime_error("Vulkan: Failed to create image buffer");
        }

        VkMemoryRequirements req;
        vkGetBufferMemoryRequirements(vulkan_driver->_device, tex_data->UploadBuffer, &req);
        VkMemoryAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = req.size;
        alloc_info.memoryTypeIndex = findMemoryType(vulkan_driver->_chosenGPU, req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        err = vkAllocateMemory(vulkan_driver->_device, &alloc_info, nullptr, &tex_data->UploadBufferMemory);
        if (err != VK_SUCCESS) {
            throw std::runtime_error("Vulkan: Failed to allocate buffer");
        }

        err = vkBindBufferMemory(vulkan_driver->_device, tex_data->UploadBuffer, tex_data->UploadBufferMemory, 0);
        if (err != VK_SUCCESS) {
            throw std::runtime_error("Vulkan: Failed to bind buffer");
        }
    }

    {
        void* map = NULL;
        err = vkMapMemory(vulkan_driver->_device, tex_data->UploadBufferMemory, 0, image_size, 0, &map);
        if (err != VK_SUCCESS) {
            throw std::runtime_error("Vulkan: Failed to create mapped image memory");
        }

        memcpy(map, image_data, image_size);
        VkMappedMemoryRange range[1] = {};
        range[0].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        range[0].memory = tex_data->UploadBufferMemory;
        range[0].size = image_size;
        err = vkFlushMappedMemoryRanges(vulkan_driver->_device, 1, range);
        if (err != VK_SUCCESS) {
            throw std::runtime_error("Vulkan: Failed to flush mapped memory range");
        }

        vkUnmapMemory(vulkan_driver->_device, tex_data->UploadBufferMemory);
    }

    stbi_image_free(image_data);

    vulkan_driver->immediate_submit([&](VkCommandBuffer cmd) {
        VkImageMemoryBarrier copy_barrier[1] = {};
        copy_barrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        copy_barrier[0].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        copy_barrier[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        copy_barrier[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        copy_barrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        copy_barrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        copy_barrier[0].image = tex_data->Image;
        copy_barrier[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy_barrier[0].subresourceRange.levelCount = 1;
        copy_barrier[0].subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, copy_barrier);

        VkBufferImageCopy region = {};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent.width = tex_data->Width;
        region.imageExtent.height = tex_data->Height;
        region.imageExtent.depth = 1;
        vkCmdCopyBufferToImage(cmd, tex_data->UploadBuffer, tex_data->Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        VkImageMemoryBarrier use_barrier[1] = {};
        use_barrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        use_barrier[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        use_barrier[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        use_barrier[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        use_barrier[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        use_barrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        use_barrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        use_barrier[0].image = tex_data->Image;
        use_barrier[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        use_barrier[0].subresourceRange.levelCount = 1;
        use_barrier[0].subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, use_barrier);
        });
}

Texture2D_Vulkan* vkTexture::TexLoadImage(void* buffer, size_t size)
{
    auto vulkan_driver = VulkanEngine::GetInstance();
    auto tex_data = CreateTexture();
    tex_data->Channels = 4;

    unsigned char* image_data = stbi_load_from_memory(
        (uint8_t*)buffer,
        (int)size,
        &tex_data->Width,
        &tex_data->Height,
        0,
        tex_data->Channels);

    if (image_data == NULL)
        throw std::runtime_error("Failed to load the image");

    InternalLoad(vulkan_driver, tex_data, image_data);

    return tex_data;
}

Texture2D_Vulkan* vkTexture::GetDummyImage()
{
    if (m_dummyTexture) {
        return m_dummyTexture.get();
    }

    m_dummyTexture = std::make_unique<Texture2D_Vulkan>();
    m_dummyTexture->Channels = 4;

    {
        // Generate file 1 Pixel Bitmap Image, with header
        std::vector<uint8_t> buffer = ImageGenerator::GenerateImage(40, 40, { 255, 255, 255, 255 });

        unsigned char* image_data = stbi_load_from_memory(
            (uint8_t*)buffer.data(),
            (int)buffer.size(),
            &m_dummyTexture->Width,
            &m_dummyTexture->Height,
            0,
            m_dummyTexture->Channels);

        if (image_data == NULL)
            throw std::runtime_error("Failed to load the image");

        auto vulkan_driver = VulkanEngine::GetInstance();

        InternalLoad(vulkan_driver, m_dummyTexture.get(), image_data);
    }

    return m_dummyTexture.get();
}

VkDescriptorSet vkTexture::GetVkDescriptorSet(Texture2D_Vulkan* handle)
{
    return handle->DS;
}

void vkTexture::QueryTexture(Texture2D_Vulkan* handle, int& outWidth, int& outHeight)
{
    outWidth = handle->Width;
    outHeight = handle->Height;
}

void vkTexture::ReleaseTexture(Texture2D_Vulkan* tex_data)
{
    if (tex_data == nullptr) {
        return;
    }

    VulkanEngine::GetInstance()->_perFrameDeletionQueue.push_function([=] {
        auto device = VulkanEngine::GetInstance()->_device;

        vkFreeMemory(device, tex_data->UploadBufferMemory, nullptr);
        vkDestroyBuffer(device, tex_data->UploadBuffer, nullptr);
        vkDestroySampler(device, tex_data->Sampler, nullptr);
        vkDestroyImageView(device, tex_data->ImageView, nullptr);
        vkDestroyImage(device, tex_data->Image, nullptr);
        vkFreeMemory(device, tex_data->ImageMemory, nullptr);

        ImGui_ImplVulkan_RemoveTexture(tex_data->DS);

        auto it = std::find_if(m_textures.begin(), m_textures.end(), [&](auto& pair) {
            return pair.second != nullptr && pair.second->Id == tex_data->Id;
            });

        if (it != m_textures.end()) {
            it->second.reset();
        }
        });
}

// This must be called from VulkanEngine!
void vkTexture::Cleanup()
{
    std::lock_guard<std::mutex> deletetion_lock(m_textureMutex);
    auto                        device = VulkanEngine::GetInstance()->_device;

    std::cout << "[Info] Cleaning up Vulkan textures: " << m_textures.size() << std::endl;

    for (auto& [id, tex_data] : m_textures) {
        if (tex_data == nullptr) {
            continue;
        }

        vkFreeMemory(device, tex_data->UploadBufferMemory, nullptr);
        vkDestroyBuffer(device, tex_data->UploadBuffer, nullptr);
        vkDestroySampler(device, tex_data->Sampler, nullptr);
        vkDestroyImageView(device, tex_data->ImageView, nullptr);
        vkDestroyImage(device, tex_data->Image, nullptr);
        vkFreeMemory(device, tex_data->ImageMemory, nullptr);

        ImGui_ImplVulkan_RemoveTexture(tex_data->DS);

        std::cout << "[Warning] Texture::Id = " << id << " automatic deleted" << std::endl;

        tex_data.reset();
    }

    if (m_dummyTexture) {
        ReleaseTexture(m_dummyTexture.get());
        m_dummyTexture.reset();
    }

    m_textures.clear();
}
