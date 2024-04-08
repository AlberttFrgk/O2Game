/*
 * MIT License
 *
 * Copyright (c) 2023 Estrol Mendex
 * See the LICENSE file in the root of this project for details.
 */

#include "VulkanBackend.h"
#include <Exceptions/EstException.h>
#include <Graphics/NativeWindow.h>
#include <Graphics/Renderer.h>
#define SDL_MAIN_HANDLED
#include <Logs.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <algorithm>
#include <array>
#include <vector>

#include <Misc/Filesystem.h>

#include "./Misc/VkNormalBlend.h"
#include "./Misc/vkinit.h"

#include "VulkanDescriptor.h"

#include "../../Shaders/SPV/image.spv.h"
#include "../../Shaders/SPV/position.spv.h"

#include "../../Shaders/SPV/image_round.spv.h"
#include "../../Shaders/SPV/position_round.spv.h"

#include "../../Shaders/SPV/sdf_text.spv.h"
#include "../../Shaders/SPV/text.spv.h"

#include "../../ImguiBackends/imgui_impl_sdl2.h"
#include "../../ImguiBackends/imgui_impl_vulkan.h"
#include <Imgui/imgui_internal.h>

#include <directxmath.h>

using namespace Graphics::Backends;

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
uint32_t           VkBlendOperatioId = 0;

struct PushConstant
{
    glm::vec4 ui_radius;
    glm::vec2 ui_size;

    glm::vec2 scale;
    glm::vec2 translate;
};

void Vulkan::Init()
{
    CreateInstance();
    InitSwapchain();
    InitCommands();
    InitSyncStructures();
    InitDescriptors();
    InitShaders();
    CreateRenderpass();
    InitPipeline();

    ImGui_Init();

    m_Initialized = true;
    m_FrameBegin = false;
}

void Graphics::Backends::Vulkan::Shutdown()
{
    if (m_Initialized) {
        Logs::Puts("[Renderer] [Vulkan] Shutting down Vulkan");
        ImGui_DeInit();

        {
            // Destroy unused resources
            Logs::Puts("[Renderer] [Vulkan] Cleaning up %d descriptors", m_Descriptors.size());

            for (auto &it : m_Descriptors) {
                DestroyDescriptor(it.get());
            }
        }

        m_PerFrameDeletionQueue.flush();
        m_SwapchainDeletionQueue.flush();

        {
            m_RenderBuffer.IndexBuffer.reset();
            m_RenderBuffer.VertexBuffer.reset();
        }

        {
            m_SamplerCache.clear();
            m_PipelineCache.clear();

            for (auto &it : m_SamplerCache) {
                vkDestroySampler(m_Device->getDevice(), it.second.sampler, nullptr);
            }

            for (auto &it : m_PipelineCache) {
                it.second.reset();
            }

            m_SamplerCache.clear();
            m_PipelineCache.clear();
        }

        m_DeletionQueue.flush();
        m_Device.reset();
    }
}

/* Vulkan Create Instance */
void Vulkan::CreateInstance()
{
    Logs::Puts("[Renderer] [Vulkan] Initializing Vulkan");
    m_Device = std::make_shared<VK_Device>();
    m_Swapchain = std::make_shared<VK_Swapchain>(m_Device.get());
    m_Renderchain = std::make_shared<VK_Renderchain>(m_Device.get());
    m_CommandPool = std::make_shared<VK_CommandPool>(
        m_Device.get(),
        m_Device->getGraphicsQueueFamily(),
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    m_DeletionQueue.push_function([=] {
        m_CommandPool.reset();
        m_Renderchain.reset();
        m_Swapchain.reset();
    });
}

bool Vulkan::InitSwapchain()
{
    auto window = Graphics::NativeWindow::Get();
    auto rect = window->GetWindowSize();

    VkExtent2D extent = {
        .width = static_cast<uint32_t>(rect.Width),
        .height = static_cast<uint32_t>(rect.Height)
    };

    bool result = m_Swapchain->createSwapchain(m_VSync, extent);
    if (result) {
        auto renderer = Graphics::Renderer::Get();
        auto scale = renderer->GetSupersampling();

        rect = window->GetBufferSize();
        extent = {
            .width = static_cast<uint32_t>(rect.Width * scale),
            .height = static_cast<uint32_t>(rect.Height * scale)
        };

        m_Renderchain->createRenderchain(
            extent,
            m_Swapchain->getFormat(),
            m_Swapchain->getDepthFormat(),
            m_Swapchain->getImageCount());

        m_SwapchainDeletionQueue.push_function([=] {
            m_Renderchain->destroyRenderchain();
        });
    }

    m_SwapchainReady = result;
    return result;
}

void Vulkan::ReInit()
{
    vkDeviceWaitIdle(m_Device->getDevice());

    if (m_SwapchainReady) {
        return;
    }

    auto window = Graphics::NativeWindow::Get();

    m_SwapchainDeletionQueue.flush();
    auto rect = window->GetWindowSize();

    VkExtent2D newSize = {
        .width = (uint32_t)rect.Width,
        .height = (uint32_t)rect.Height
    };

    m_Swapchain->createSwapchain(m_VSync, newSize);

    if (!m_Swapchain->isReady()) {
        m_SwapchainReady = false;
        return;
    }

    rect = window->GetBufferSize();
    newSize = {
        .width = static_cast<uint32_t>(rect.Width),
        .height = static_cast<uint32_t>(rect.Height)
    };

    m_Renderchain->createRenderchain(
        newSize,
        m_Swapchain->getFormat(),
        m_Swapchain->getDepthFormat(),
        m_Swapchain->getImageCount());

    m_SwapchainDeletionQueue.push_function([=] {
        m_Renderchain->destroyRenderchain();
    });

    CreateRenderpass();
    InitSyncStructures();

    m_SwapchainReady = true;
}

void Vulkan::InitCommands()
{
    m_Frames.resize(MAX_FRAMES_IN_FLIGHT);

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        m_Frames[i].commandBuffer = m_CommandPool->allocateCommandBuffer();

        m_DeletionQueue.push_function([=] {
            auto cmd = m_Frames[i].commandBuffer;
            m_CommandPool->freeCommandBuffer(cmd);
        });
    }
}

void Vulkan::CreateRenderpass()
{
    VkFormat _swaphainImageFormat = m_Swapchain->getFormat();
    VkFormat _depthFormat = vkinit::get_supported_depth_format(m_Device->getGPUDevice());

    VkAttachmentDescription color_attachment = {};
    color_attachment.format = _swaphainImageFormat;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_ref = {};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depth_attachment = {};
    depth_attachment.flags = 0;
    depth_attachment.format = _depthFormat;
    depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_attachment_ref = {};
    depth_attachment_ref.attachment = 1;
    depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;
    subpass.pDepthStencilAttachment = &depth_attachment_ref;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkSubpassDependency depth_dependency = {};
    depth_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    depth_dependency.dstSubpass = 0;
    depth_dependency.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    depth_dependency.srcAccessMask = 0;
    depth_dependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    depth_dependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkSubpassDependency dependencies[2] = { dependency, depth_dependency };

    VkAttachmentDescription attachments[2] = { color_attachment, depth_attachment };

    VkRenderPassCreateInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 2;
    render_pass_info.pAttachments = &attachments[0];
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 2;
    render_pass_info.pDependencies = &dependencies[0];

    // Renderpass context
    {
        VkExtent2D newSize = m_Renderchain->getExtent();

        VkFramebufferCreateInfo fb_info = vkinit::framebuffer_create_info(VK_NULL_HANDLE, newSize);

        std::vector<VkImageView> images;
        auto                    &renderImages = m_Renderchain->getImages();

        for (uint32_t i = 0; i < (uint32_t)renderImages.size(); i++) {
            images.push_back(renderImages[i]->getView());
        }

        VkImageView depthAttachment = m_Renderchain->getDepthImage()->getView();
        uint32_t    imageCount = (uint32_t)renderImages.size();

        m_RenderContext.renderpass = std::make_shared<VK_Renderpass>(
            render_pass_info,
            fb_info,
            m_Device.get(),
            images,
            depthAttachment,
            imageCount);

        m_RenderContext.extent = {
            .width = newSize.width,
            .height = newSize.height,
            .depth = 1
        };
    }

    // Present context
    {
        VkExtent2D newSize = m_Swapchain->getExtent();

        VkFramebufferCreateInfo fb_info = vkinit::framebuffer_create_info(VK_NULL_HANDLE, newSize);

        std::vector<VkImageView> images;
        auto                    &swapchainImages = m_Swapchain->getImageViews();
        for (uint32_t i = 0; i < (uint32_t)swapchainImages.size(); i++) {
            images.push_back(swapchainImages[i]);
        }

        VkImageView depthAttachment = m_Swapchain->getDepthImage()->getView();
        uint32_t    imageCount = (uint32_t)swapchainImages.size();

        m_PresentContext.renderpass = std::make_shared<VK_Renderpass>(
            render_pass_info,
            fb_info,
            m_Device.get(),
            images,
            depthAttachment,
            imageCount);

        m_PresentContext.extent = {
            .width = newSize.width,
            .height = newSize.height,
            .depth = 1
        };
    }

    {
        bool bothIsSameExtent = m_RenderContext.extent.width == m_PresentContext.extent.width &&
                                m_RenderContext.extent.height == m_PresentContext.extent.height;

        m_BothRenderAndPresent = bothIsSameExtent;
    }

    m_SwapchainDeletionQueue.push_function([=] {
        m_RenderContext.renderpass.reset();
        m_PresentContext.renderpass.reset();
    });
}

void Vulkan::InitSyncStructures()
{
    VkFenceCreateInfo     fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        auto result = vkCreateFence(m_Device->getDevice(), &fenceCreateInfo, nullptr, &m_Frames[i].sync.renderFence);

        if (result != VK_SUCCESS) {
            throw Exceptions::EstException("Failed to create fence");
        }

        result = vkCreateSemaphore(m_Device->getDevice(), &semaphoreCreateInfo, nullptr, &m_Frames[i].sync.presentSemaphore);

        if (result != VK_SUCCESS) {
            throw Exceptions::EstException("Failed to create semaphore");
        }

        result = vkCreateSemaphore(m_Device->getDevice(), &semaphoreCreateInfo, nullptr, &m_Frames[i].sync.renderSemaphore);

        if (result != VK_SUCCESS) {
            throw Exceptions::EstException("Failed to create semaphore");
        }

        m_SwapchainDeletionQueue.push_function([=]() {
            vkDestroyFence(m_Device->getDevice(), m_Frames[i].sync.renderFence, nullptr);
            vkDestroySemaphore(m_Device->getDevice(), m_Frames[i].sync.presentSemaphore, nullptr);
            vkDestroySemaphore(m_Device->getDevice(), m_Frames[i].sync.renderSemaphore, nullptr);
        });
    }
}

void Vulkan::InitDescriptors()
{
    std::vector<VkDescriptorPoolSize> sizes = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1000;
    poolInfo.poolSizeCount = (uint32_t)sizes.size();
    poolInfo.pPoolSizes = sizes.data();

    VkDescriptorPool descriptorPool;

    auto result = vkCreateDescriptorPool(
        m_Device->getDevice(),
        &poolInfo,
        nullptr,
        &descriptorPool);

    if (result != VK_SUCCESS) {
        throw Exceptions::EstException("Failed to create descriptor pool");
    }

    VkDescriptorSetLayoutBinding binding[1] = {
        { .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT }
    };

    VkDescriptorSetLayoutCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = binding
    };

    VkDescriptorSetLayout descriptorSetLayout;

    result = vkCreateDescriptorSetLayout(
        m_Device->getDevice(),
        &info,
        nullptr,
        &descriptorSetLayout);

    if (result != VK_SUCCESS) {
        throw Exceptions::EstException("Failed to create descriptor set layout");
    }

    m_Descriptor.layout = descriptorSetLayout;
    m_Descriptor.pool = descriptorPool;

    m_DeletionQueue.push_function([=] {
        vkDestroyDescriptorSetLayout(m_Device->getDevice(), m_Descriptor.layout, nullptr);
        vkDestroyDescriptorPool(m_Device->getDevice(), m_Descriptor.pool, nullptr);

        m_Descriptor.layout = VK_NULL_HANDLE;
        m_Descriptor.pool = VK_NULL_HANDLE;
    });
}

void Vulkan::InitShaders()
{
}

void Vulkan::InitPipeline()
{
    VkResult result = VK_SUCCESS;

    VkDescriptorSetLayout image_descriptor_layout;
    {
        VkDescriptorSetLayoutBinding binding[1] = {};
        binding[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binding[0].descriptorCount = 1;
        binding[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        VkDescriptorSetLayoutCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.bindingCount = 1;
        info.pBindings = binding;

        result = vkCreateDescriptorSetLayout(
            m_Device->getDevice(),
            &info,
            nullptr,
            &image_descriptor_layout);

        if (result != VK_SUCCESS) {
            throw Exceptions::EstException("Failed to create descriptor set layout");
        }
    }

    VkPipelineLayout pipeline_layout;
    {
        VkPushConstantRange push_constants[1] = {};
        push_constants[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        push_constants[0].offset = 0;
        push_constants[0].size = sizeof(PushConstant);
        VkDescriptorSetLayout      set_layout[1] = { image_descriptor_layout };
        VkPipelineLayoutCreateInfo layout_info = {};
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.setLayoutCount = 1;
        layout_info.pSetLayouts = set_layout;
        layout_info.pushConstantRangeCount = 1;
        layout_info.pPushConstantRanges = push_constants;

        result = vkCreatePipelineLayout(
            m_Device->getDevice(),
            &layout_info,
            nullptr,
            &pipeline_layout);

        if (result != VK_SUCCESS) {
            throw Exceptions::EstException("Failed to create pipeline layout");
        }
    }

    m_Pipeline.layout = image_descriptor_layout;
    m_Pipeline.pipelineLayout = pipeline_layout;

    m_DeletionQueue.push_function([=] {
        vkDestroyDescriptorSetLayout(m_Device->getDevice(), image_descriptor_layout, nullptr);
        vkDestroyPipelineLayout(m_Device->getDevice(), pipeline_layout, nullptr);

        m_Pipeline.layout = VK_NULL_HANDLE;
        m_Pipeline.pipelineLayout = VK_NULL_HANDLE;
    });

    // clang-format off
    Graphics::Backends::PipelineInfo pipelineInfo = {
        .IsFile = false,
        .VertexShader = {
            .Memory = {
                .Code = __glsl_position,
                .CodeSize = sizeof(__glsl_position) / sizeof(__glsl_position[0])
            }
        },
        .FragmentShader = {
            .Memory = {
                .Code = __glsl_image,
                .CodeSize = sizeof(__glsl_image) / sizeof(__glsl_image[0])
            }
        },
        .EntryPoint = "main",
        .BlendInfo = {
            .Enable = true,
            .SrcColor = Graphics::Backends::BlendFactor::BLEND_FACTOR_SRC_ALPHA,
            .DstColor = Graphics::Backends::BlendFactor::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .ColorOp = Graphics::Backends::BlendOp::BLEND_OP_ADD,
            .SrcAlpha = Graphics::Backends::BlendFactor::BLEND_FACTOR_SRC_ALPHA,
            .DstAlpha = Graphics::Backends::BlendFactor::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .AlphaOp = Graphics::Backends::BlendOp::BLEND_OP_ADD
        }
    };
    // clang-format on

    m_FallbackPipeline = CreatePipeline(pipelineInfo);
}

void Vulkan::ImmediateSubmit(std::function<void(VkCommandBuffer cmd)> &&function)
{
    VkCommandBuffer cmd = m_CommandPool->allocateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    function(cmd);

    vkEndCommandBuffer(cmd);

    VkFence           fence;
    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = 0;

    auto result = vkCreateFence(m_Device->getDevice(), &fenceInfo, nullptr, &fence);

    if (result != VK_SUCCESS) {
        throw Exceptions::EstException("Failed to create fence");
    }

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    result = vkQueueSubmit(m_Device->getGraphicsQueue(), 1, &submitInfo, fence);

    if (result != VK_SUCCESS) {
        throw Exceptions::EstException("Failed to submit command buffer");
    }

    vkWaitForFences(m_Device->getDevice(), 1, &fence, VK_TRUE, UINT64_MAX);
    vkDestroyFence(m_Device->getDevice(), fence, nullptr);

    m_CommandPool->freeCommandBuffer(cmd, false);
}

void Vulkan::AsyncSubmit(std::function<void(VkCommandBuffer cmd)> &&function)
{
    ImmediateSubmit(std::move(function));
}

bool Vulkan::BeginFrame()
{
    if (!m_SwapchainReady) {
        return false;
    }

    auto &frame = GetCurrentFrame();

    auto result = vkDeviceWaitIdle(m_Device->getDevice());
    if (result != VK_SUCCESS) {
        throw Exceptions::EstException("Failed to wait for device idle");
    }

    result = vkWaitForFences(m_Device->getDevice(), 1, &frame.sync.renderFence, VK_TRUE, UINT64_MAX);

    if (result == VK_TIMEOUT) {
        return false;
    } else if (result != VK_SUCCESS) {
        throw Exceptions::EstException("Failed to wait for fence");
    }

    m_PerFrameDeletionQueue.flush();

    m_Swapchain->accquireNextImage(m_SwapchainIndex, frame.sync.presentSemaphore, VK_NULL_HANDLE);
    if (!m_Swapchain->isReady()) {
        m_SwapchainReady = false;
        return false;
    }

    result = vkResetFences(m_Device->getDevice(), 1, &frame.sync.renderFence);

    if (result != VK_SUCCESS) {
        throw Exceptions::EstException("Failed to reset fence");
    }

    VkCommandBufferBeginInfo beginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);
    result = vkBeginCommandBuffer(frame.commandBuffer, &beginInfo);

    if (result != VK_SUCCESS) {
        throw Exceptions::EstException("Failed to begin command buffer");
    }

    VkClearValue clearValues;
    clearValues.color = { 0.0f, 0.0f, 0.0f, 1.0f };

    VkClearValue depthClear;
    depthClear.depthStencil = { 1.0f, 0 };

    VkExtent2D newSize = m_Renderchain->getExtent();
    m_RenderContext.renderpass->setClearValue(clearValues);
    m_RenderContext.renderpass->setDepthClearValue(depthClear);
    m_RenderContext.renderpass->setExtent(newSize);

    newSize = m_Swapchain->getExtent();
    m_PresentContext.renderpass->setClearValue(clearValues);
    m_PresentContext.renderpass->setDepthClearValue(depthClear);
    m_PresentContext.renderpass->setExtent(newSize);

    m_PresentContext.renderpass->Begin(frame.commandBuffer, m_SwapchainIndex);

    if (!m_BothRenderAndPresent) {
        m_PresentContext.renderpass->End(frame.commandBuffer);
        m_RenderContext.renderpass->Begin(frame.commandBuffer, m_SwapchainIndex);
    }

    m_FrameBegin = true;
    return true;
}

void Vulkan::ImageBarrier(VkCommandBuffer cmdbuffer,
                          VkImage image, VkImageLayout oldLayout,
                          VkImageLayout newLayout, VkImageAspectFlags aspectMask)
{
    VkImageMemoryBarrier barrierInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT,
        .dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {
            .aspectMask = aspectMask,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount = VK_REMAINING_ARRAY_LAYERS,
        }
    };

    vkCmdPipelineBarrier(
        cmdbuffer,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // srcStageMask
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // dstStageMask
        0,                                  // dependencyFlags
        0, nullptr,                         // memoryBarrierCount, pMemoryBarriers
        0, nullptr,                         // bufferMemoryBarrierCount, pBufferMemoryBarriers
        1, &barrierInfo                     // imageMemoryBarrierCount, pImageMemoryBarriers
    );
}

void Vulkan::EndFrame()
{
    if (!m_FrameBegin) {
        return;
    }

    auto &frame = GetCurrentFrame();
    FlushQueue();

    if (m_HasImgui) {
        ImDrawData *draw_data = ImGui::GetDrawData();
        ImGui_ImplVulkan_RenderDrawData(draw_data, frame.commandBuffer);
    }

    if (!m_BothRenderAndPresent) {
        m_RenderContext.renderpass->End(frame.commandBuffer);

        // blit to swapchain
        VkImage srcImage = m_Renderchain->getImages()[m_SwapchainIndex]->getImage();
        VkImage dstImage = m_Swapchain->getImages()[m_SwapchainIndex];

        VkOffset3D originSrcSize;
        originSrcSize.x = m_RenderContext.extent.width;
        originSrcSize.y = m_RenderContext.extent.height;
        originSrcSize.z = 1;

        VkOffset3D originDstSize;
        originDstSize.x = m_PresentContext.extent.width;
        originDstSize.y = m_PresentContext.extent.height;
        originDstSize.z = 1;

        VkImageBlit imageBlitRegion{};
        imageBlitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageBlitRegion.srcSubresource.layerCount = 1;
        imageBlitRegion.srcOffsets[1] = originSrcSize;
        imageBlitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageBlitRegion.dstSubresource.layerCount = 1;
        imageBlitRegion.dstOffsets[1] = originDstSize;

        {
            ImageBarrier(
                frame.commandBuffer,
                srcImage,
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_IMAGE_ASPECT_COLOR_BIT);

            ImageBarrier(
                frame.commandBuffer,
                dstImage,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_ASPECT_COLOR_BIT);

            vkCmdBlitImage(
                frame.commandBuffer,
                srcImage,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                dstImage,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &imageBlitRegion,
                VK_FILTER_LINEAR);

            ImageBarrier(
                frame.commandBuffer,
                dstImage,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                VK_IMAGE_ASPECT_COLOR_BIT);
        }
    } else {
        m_PresentContext.renderpass->End(frame.commandBuffer);
    }

    auto result = vkEndCommandBuffer(frame.commandBuffer);

    if (result != VK_SUCCESS) {
        throw Exceptions::EstException("Failed to end command buffer");
    }

    VkSubmitInfo         submit = vkinit::submit_info(&frame.commandBuffer);
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    submit.pWaitDstStageMask = &waitStage;
    submit.waitSemaphoreCount = 1;
    submit.pWaitDstStageMask = &waitStage;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &frame.sync.presentSemaphore;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &frame.sync.renderSemaphore;

    result = vkQueueSubmit(m_Device->getGraphicsQueue(), 1, &submit, frame.sync.renderFence);

    if (result != VK_SUCCESS) {
        throw Exceptions::EstException("Failed to submit queue");
    }

    VkSwapchainKHR swapchain = m_Swapchain->getSwapchain();

    VkPresentInfoKHR presentInfo = vkinit::present_info();
    presentInfo.pSwapchains = &swapchain;
    presentInfo.swapchainCount = 1;
    presentInfo.pWaitSemaphores = &frame.sync.renderSemaphore;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pImageIndices = &m_SwapchainIndex;

    m_Swapchain->presentImage(presentInfo);

    vkQueueWaitIdle(m_Device->getGraphicsQueue());
    m_FrameBegin = false;
}

void Vulkan::Push(SubmitInfo &info)
{
    if (info.vertices.size() == 0) {
        return;
    }

    if (info.uiSize.x == 0 || info.uiSize.y == 0) {
        return;
    }

    uint32_t maxSize = (m_RenderDrawData.vertexSize + static_cast<uint32_t>(info.vertices.size())) * sizeof(info.vertices[0]);
    uint32_t inMaxSize = (m_RenderDrawData.indiceSize + static_cast<uint32_t>(info.indices.size())) * sizeof(info.indices[0]);

    if (maxSize >= m_RenderBuffer.maxVertexBufferSize || inMaxSize >= m_RenderBuffer.maxIndexBufferSize) {
        ResizeBuffer(maxSize, inMaxSize);
    }

    for (auto &it : info.vertices) {
        m_RenderDrawData.vertexPtr[m_RenderDrawData.vertexSize++] = it;
    }

    for (auto &it : info.indices) {
        m_RenderDrawData.indexPtr[m_RenderDrawData.indiceSize++] = it;
    }

    VulkanDrawItem item = {};
    item.pipeline = info.pipeline;
    item.count = (uint32_t)info.indices.size();
    item.instanceCount = 1;
    item.uiRadius = info.uiRadius;
    item.image = info.image;
    item.uiSize = info.uiSize;
    item.clipRect = info.clipRect;

    submitInfos.push_back(item);
}

void Vulkan::Push(std::vector<SubmitInfo> &infos)
{
    if (infos.size() == 0) {
        return;
    }

    if (infos[0].uiSize.x == 0 || infos[0].uiSize.y == 0) {
        return;
    }

    size_t size = infos.size();

    uint32_t maxSize = (m_RenderDrawData.vertexSize + static_cast<uint32_t>(infos[0].vertices.size() * size)) * sizeof(infos[0].vertices[0]);
    uint32_t inMaxSize = (m_RenderDrawData.indiceSize + static_cast<uint32_t>(infos[0].indices.size() * size)) * sizeof(infos[0].indices[0]);

    if (maxSize >= m_RenderBuffer.maxVertexBufferSize || inMaxSize >= m_RenderBuffer.maxIndexBufferSize) {
        ResizeBuffer(maxSize, inMaxSize);
    }

    uint16_t index_increment = 0;
    for (auto &info : infos) {
        for (auto &it : info.vertices) {
            m_RenderDrawData.vertexPtr[m_RenderDrawData.vertexSize++] = it;
        }

        for (auto &it : info.indices) {
            m_RenderDrawData.indexPtr[m_RenderDrawData.indiceSize++] = index_increment + it;
        }

        index_increment += (uint16_t)info.indices.size();
    }

    VulkanDrawItem item = {};
    item.count = (uint32_t)infos[0].indices.size();
    item.instanceCount = (uint32_t)size;
    item.pipeline = infos[0].pipeline;
    item.uiRadius = infos[0].uiRadius;
    item.image = infos[0].image;
    item.uiSize = infos[0].uiSize;
    item.clipRect = infos[0].clipRect;

    submitInfos.push_back(item);
}

void Vulkan::FlushQueue()
{
    if (!submitInfos.size()) {
        return;
    }

    if (!m_SwapchainReady) {
        submitInfos.clear();
        return;
    }

    auto &frame = GetCurrentFrame();
    auto  renderRect = m_Renderchain->getExtent();

    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = (float)renderRect.width;
    viewport.height = (float)renderRect.height;
    viewport.minDepth = 1.0f;

    vkCmdSetViewport(frame.commandBuffer, 0, 1, &viewport);

    auto vertexBuffer = m_RenderBuffer.VertexBuffer->getBuffer();
    auto indexBuffer = m_RenderBuffer.IndexBuffer->getBuffer();

    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(frame.commandBuffer, 0, 1, &vertexBuffer, offsets);
    vkCmdBindIndexBuffer(frame.commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);

    uint32_t currentVertIndex = 0; // vertex
    uint32_t currentIndiIndex = 0; // indicies

    PushConstant pc = {};

    pc.scale = glm::vec2(2.0f / renderRect.width, 2.0f / renderRect.height);
    pc.translate = glm::vec2(-1.0f, -1.0f);

    for (auto &info : submitInfos) {
        auto                        &pipeline = m_Pipeline.pipelineLayout;
        std::shared_ptr<VK_Pipeline> graphics;

        PipelineHandle pipelineHandle = info.pipeline;
        if (m_PipelineCache.find(pipelineHandle) == m_PipelineCache.end()) {
            graphics = m_PipelineCache[m_FallbackPipeline];
        } else {
            graphics = m_PipelineCache[pipelineHandle];
        }

        VkRect2D clip = {};
        clip.offset = {
            std::max(0, info.clipRect.X),
            std::max(0, info.clipRect.Y)
        };
        clip.extent = {
            static_cast<uint32_t>(std::max(0, info.clipRect.Width - info.clipRect.X)),
            static_cast<uint32_t>(std::max(0, info.clipRect.Height - info.clipRect.Y))
        };

        if (clip.extent.width == 0 || clip.extent.height == 0) {
            continue; // there no need to draw
        }

        pc.ui_size = info.uiSize;
        pc.ui_radius = info.uiRadius;

        vkCmdPushConstants(frame.commandBuffer, pipeline, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstant), &pc);

        VkDescriptorSet image = (VkDescriptorSet)(info.image != 0 ? (void *)info.image : VK_NULL_HANDLE);
        uint32_t        imageCount = 1;
        if (image == NULL) {
            imageCount = 0;
        }

        vkCmdBindDescriptorSets(frame.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline, 0, 1, &image, 0, nullptr);
        graphics->Bind(frame.commandBuffer);

        vkCmdSetScissor(frame.commandBuffer, 0, 1, &clip);

        vkCmdDrawIndexed(
            frame.commandBuffer,
            info.count * info.instanceCount,
            1,
            currentIndiIndex,
            currentVertIndex,
            0);

        currentVertIndex += (int)info.count * info.instanceCount;
        currentIndiIndex += (int)info.count * info.instanceCount;
    }

    submitInfos.clear();
    m_RenderDrawData.Reset();
}

void Vulkan::ResizeBuffer(VkDeviceSize vertices, VkDeviceSize indices)
{
    VkResult result = VK_SUCCESS;

    if (vertices > m_RenderBuffer.maxVertexBufferSize) {
        m_RenderBuffer.maxVertexBufferSize = (uint32_t)vertices;

        std::shared_ptr<VK_Memory> vertexBuffer = std::make_shared<VK_Memory>(
            m_Device.get(),
            vertices,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            (VkMemoryPropertyFlags)(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));

        bool hasPreviousData = m_RenderDrawData.vertexSize > 0;

        std::vector<Vertex> backupVertices;
        if (hasPreviousData) {
            backupVertices.resize(m_RenderDrawData.vertexSize);
            memcpy(backupVertices.data(), m_RenderDrawData.vertexPtr, m_RenderDrawData.vertexSize * sizeof(Vertex));
        }

        m_RenderBuffer.VertexBuffer = vertexBuffer;
        m_RenderDrawData.vertexPtr = (Vertex *)m_RenderBuffer.VertexBuffer->Data();

        if (hasPreviousData) {
            memcpy(m_RenderDrawData.vertexPtr, backupVertices.data(), m_RenderDrawData.vertexSize * sizeof(Vertex));
        }
    }

    if (indices > m_RenderBuffer.maxIndexBufferSize) {
        m_RenderBuffer.maxIndexBufferSize = (uint32_t)indices;

        std::shared_ptr<VK_Memory> indexBuffer = std::make_shared<VK_Memory>(
            m_Device.get(),
            indices,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            (VkMemoryPropertyFlags)(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));

        bool hasPreviousData = m_RenderDrawData.indiceSize > 0;

        std::vector<uint16_t> backupIndices;
        if (hasPreviousData) {
            backupIndices.resize(m_RenderDrawData.indiceSize);
            memcpy(backupIndices.data(), m_RenderDrawData.indexPtr, m_RenderDrawData.indiceSize * sizeof(uint16_t));
        }

        m_RenderBuffer.IndexBuffer = indexBuffer;
        m_RenderDrawData.indexPtr = (uint16_t *)m_RenderBuffer.IndexBuffer->Data();

        if (hasPreviousData) {
            memcpy(m_RenderDrawData.indexPtr, backupIndices.data(), m_RenderDrawData.indiceSize * sizeof(uint16_t));
        }
    }
}

VK_Device *Vulkan::GetDevice()
{
    return m_Device.get();
}

VK_Swapchain *Vulkan::GetSwapchain()
{
    return m_Swapchain.get();
}

VulkanFrame &Vulkan::GetCurrentFrame()
{
    return m_Frames[m_CurrentFrame % MAX_FRAMES_IN_FLIGHT];
}

VulkanFrame &Vulkan::GetLastFrame()
{
    return m_Frames[(m_CurrentFrame - 1) % MAX_FRAMES_IN_FLIGHT];
}

void Vulkan::SetVSync(bool vsync)
{
    m_VSync = vsync;
    m_SwapchainReady = false; // force swapchain re-creation
}

bool Vulkan::NeedReinit()
{
    return !m_SwapchainReady;
}

VulkanDescriptor *Vulkan::CreateDescriptor()
{
    auto descriptor = std::make_unique<VulkanDescriptor>();
    descriptor->Id = m_DescriptorId++;

    m_Descriptors.push_back(std::move(descriptor));
    return m_Descriptors.back().get();
}

std::string HashSamplerInfo(Graphics::TextureSamplerInfo SamplerInfo)
{
    std::string hash = std::to_string((int)SamplerInfo.FilterMag) + std::to_string((int)SamplerInfo.FilterMin) +
                       std::to_string((int)SamplerInfo.AddressModeU) + std::to_string((int)SamplerInfo.AddressModeV) +
                       std::to_string((int)SamplerInfo.AddressModeW) + std::to_string(SamplerInfo.MipLodBias) +
                       std::to_string(SamplerInfo.AnisotropyEnable) + std::to_string(SamplerInfo.MaxAnisotropy) +
                       std::to_string(SamplerInfo.CompareEnable) + std::to_string((int)SamplerInfo.CompareOp) +
                       std::to_string(SamplerInfo.MinLod) + std::to_string(SamplerInfo.MaxLod);

    std::hash<std::string> hasher;
    return std::to_string(hasher(hash));
}

VkSampler Vulkan::CreateSampler(Graphics::TextureSamplerInfo SamplerInfo)
{
    auto hash = HashSamplerInfo(SamplerInfo);
    if (m_SamplerCache.find(hash) != m_SamplerCache.end()) {
        return m_SamplerCache[hash].sampler;
    }

    VkSamplerCreateInfo samplerInfo = {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    switch (SamplerInfo.FilterMag) {
        case TextureFilter::Nearest:
            samplerInfo.magFilter = VK_FILTER_NEAREST;
            break;

        case TextureFilter::Linear:
            samplerInfo.magFilter = VK_FILTER_LINEAR;
            break;
    }

    switch (SamplerInfo.FilterMin) {
        case TextureFilter::Nearest:
            samplerInfo.minFilter = VK_FILTER_NEAREST;
            break;

        case TextureFilter::Linear:
            samplerInfo.minFilter = VK_FILTER_LINEAR;
            break;
    }

    switch (SamplerInfo.AddressModeU) {
        case TextureAddressMode::Repeat:
            samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            break;

        case TextureAddressMode::ClampEdge:
            samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            break;

        case TextureAddressMode::ClampBorder:
            samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
            break;

        case TextureAddressMode::MirrorRepeat:
            samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
            break;

        case TextureAddressMode::MirrorClampEdge:
            samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
            break;
    }

    switch (SamplerInfo.AddressModeV) {
        case TextureAddressMode::Repeat:
            samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            break;

        case TextureAddressMode::ClampEdge:
            samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            break;

        case TextureAddressMode::ClampBorder:
            samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
            break;

        case TextureAddressMode::MirrorRepeat:
            samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
            break;

        case TextureAddressMode::MirrorClampEdge:
            samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
            break;
    }

    switch (SamplerInfo.AddressModeW) {
        case TextureAddressMode::Repeat:
            samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            break;

        case TextureAddressMode::ClampEdge:
            samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            break;

        case TextureAddressMode::ClampBorder:
            samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
            break;

        case TextureAddressMode::MirrorRepeat:
            samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
            break;

        case TextureAddressMode::MirrorClampEdge:
            samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
            break;
    }

    samplerInfo.mipLodBias = SamplerInfo.MipLodBias;
    samplerInfo.anisotropyEnable = SamplerInfo.AnisotropyEnable;
    samplerInfo.maxAnisotropy = SamplerInfo.MaxAnisotropy;
    samplerInfo.compareEnable = SamplerInfo.CompareEnable;

    switch (SamplerInfo.CompareOp) {
        case TextureCompareOP::COMPARE_OP_ALWAYS:
            samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
            break;

        case TextureCompareOP::COMPARE_OP_NEVER:
            samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
            break;

        case TextureCompareOP::COMPARE_OP_LESS:
            samplerInfo.compareOp = VK_COMPARE_OP_LESS;
            break;

        case TextureCompareOP::COMPARE_OP_EQUAL:
            samplerInfo.compareOp = VK_COMPARE_OP_EQUAL;
            break;

        case TextureCompareOP::COMPARE_OP_LESS_OR_EQUAL:
            samplerInfo.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
            break;

        case TextureCompareOP::COMPARE_OP_GREATER:
            samplerInfo.compareOp = VK_COMPARE_OP_GREATER;
            break;

        case TextureCompareOP::COMPARE_OP_NOT_EQUAL:
            samplerInfo.compareOp = VK_COMPARE_OP_NOT_EQUAL;
            break;

        case TextureCompareOP::COMPARE_OP_GREATER_OR_EQUAL:
            samplerInfo.compareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
            break;

        case TextureCompareOP::COMPARE_OP_MAX_ENUM:
            samplerInfo.compareOp = VK_COMPARE_OP_MAX_ENUM;
            break;
    }

    samplerInfo.minLod = SamplerInfo.MinLod;
    samplerInfo.maxLod = SamplerInfo.MaxLod;

    VkBorderColor borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.borderColor = borderColor;

    VkSampler sampler;
    auto      result = vkCreateSampler(m_Device->getDevice(), &samplerInfo, nullptr, &sampler);

    if (result != VK_SUCCESS) {
        throw Exceptions::EstException("Failed to create sampler");
    }

    VulkanSampler vulkanSampler = {
        .hash = hash,
        .sampler = sampler
    };

    m_SamplerCache[hash] = vulkanSampler;
    return sampler;
}

VulkanDescriptorPool *Vulkan::GetDescriptorPool()
{
    return &m_Descriptor;
}

void Vulkan::DestroyDescriptor(VulkanDescriptor *descriptor, bool _delete)
{
    if (!descriptor) {
        return;
    }

    auto found = std::find_if(m_Descriptors.begin(), m_Descriptors.end(), [&](auto &it) {
        return it->ImageData->getId() == descriptor->ImageData->getId();
    });

    VulkanDescriptor *desc = found->release();
    int               Id = desc->ImageData->getId();

    Logs::Debug("Deleting a image with id: %d, now has %d remaining", Id, m_Descriptors.size());

    if (found != m_Descriptors.end()) {
        m_Descriptors.erase(found);
    } else {
        __debugbreak();
    }

    m_PerFrameDeletionQueue.push_function([&, desc] {
        desc->UploadBuffer.reset();

        auto device = m_Device->getDevice();

        if (desc->VkId != VK_NULL_HANDLE) {
            auto pool = GetDescriptorPool();
            vkFreeDescriptorSets(device, pool->pool, 1, &desc->VkId);
        }

        if (desc->ImageData.use_count() > 1) {
            Logs::Debug("Warn: %d", desc->ImageData->getImage());
        }

        desc->ImageData.reset();
    });
}

std::string HashPipelineInfo(PipelineInfo &info)
{
    std::string content;
    {
        content += std::to_string(info.IsFile);
        if (info.IsFile) {
            content += std::string(info.VertexShader.File);
            content += std::string(info.FragmentShader.File);
        } else {
            std::hash<std::string> hasher;
            std::string            vertexContent = std::string(
                (const char *)info.VertexShader.Memory.Code,
                info.VertexShader.Memory.CodeSize * sizeof(uint32_t));

            std::string fragmentContent = std::string(
                (const char *)info.FragmentShader.Memory.Code,
                info.FragmentShader.Memory.CodeSize * sizeof(uint32_t));

            content += std::to_string(hasher(vertexContent));
            content += std::to_string(hasher(fragmentContent));
        }

        content += info.EntryPoint;
    }

    {
        std::string blend = std::to_string(info.BlendInfo.Enable);
        blend += std::to_string((int)info.BlendInfo.SrcColor);
        blend += std::to_string((int)info.BlendInfo.DstColor);
        blend += std::to_string((int)info.BlendInfo.ColorOp);
        blend += std::to_string((int)info.BlendInfo.SrcAlpha);
        blend += std::to_string((int)info.BlendInfo.DstAlpha);
        blend += std::to_string((int)info.BlendInfo.AlphaOp);

        content += blend;
    }

    std::hash<std::string> hasher;
    return std::to_string(hasher(content));
}

PipelineHandle Vulkan::CreatePipeline(PipelineInfo &info)
{
    auto hash = HashPipelineInfo(info);
    if (m_PipelineHash.find(hash) != m_PipelineHash.end()) {
        return m_PipelineHash[hash];
    }

    std::shared_ptr<VK_Shader> shader;
    {
        if (info.IsFile) {
            std::vector<uint32_t> vertexShader = Misc::Filesystem::ReadFile32(info.VertexShader.File);
            std::vector<uint32_t> fragmentShader = Misc::Filesystem::ReadFile32(info.FragmentShader.File);

            shader = std::make_shared<VK_Shader>(
                m_Device.get(),
                vertexShader.data(), fragmentShader.data(),
                vertexShader.size(), fragmentShader.size());
        } else {
            const uint32_t *vertexShader = reinterpret_cast<const uint32_t *>(info.VertexShader.Memory.Code);
            const uint32_t *fragmentShader = reinterpret_cast<const uint32_t *>(info.FragmentShader.Memory.Code);

            size_t vertexSize = info.VertexShader.Memory.CodeSize;
            size_t fragmentSize = info.FragmentShader.Memory.CodeSize;

            shader = std::make_shared<VK_Shader>(
                m_Device.get(),
                vertexShader, fragmentShader,
                vertexSize, fragmentSize);
        }
    }

    static PipelineHandle VkPipelineId = 0;
    PipelineHandle        pipelineId = VkPipelineId++;

    auto pipeline = std::make_shared<VK_Pipeline>(
        m_Device.get(),
        m_Pipeline.pipelineLayout,
        m_RenderContext.renderpass->getRenderpass(),
        shader.get(),
        info.EntryPoint,
        info.BlendInfo);

    m_PipelineHash[hash] = pipelineId;
    m_PipelineCache[pipelineId] = pipeline;

    return pipelineId;
}

// TODO: implement later
void Vulkan::ImGui_Init()
{
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    VkDescriptorPool imguiPool;
    auto             result = vkCreateDescriptorPool(
        m_Device->getDevice(),
        &pool_info,
        nullptr,
        &imguiPool);

    if (result != VK_SUCCESS) {
        throw Exceptions::EstException("Failed to init imgui descriptor pool");
    }

    ImGui::CreateContext();

    auto window = Graphics::NativeWindow::Get();
    ImGui_ImplSDL2_InitForVulkan(window->GetWindow());
    window->AddSDLCallback([=](SDL_Event &ev) {
        ImGui_ImplSDL2_ProcessEvent(&ev);
    });

    auto &IO = ImGui::GetIO();
    auto  size = window->GetBufferSize();
    IO.DisplaySize = ImVec2((float)size.Width, (float)size.Height);
    IO.IniFilename = NULL;

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = m_Device->getInstance();
    init_info.PhysicalDevice = m_Device->getGPUDevice();
    init_info.Device = m_Device->getDevice();
    init_info.Queue = m_Device->getGraphicsQueue();
    init_info.DescriptorPool = imguiPool;
    init_info.MinImageCount = 3;
    init_info.ImageCount = 3;
    init_info.MSAASamples = m_Device->getMSAASampleCount();
    init_info.RenderPass = m_RenderContext.renderpass->getRenderpass();

    ImGui_ImplVulkan_Init(&init_info);

    m_Imgui.imguiPool = imguiPool;
}

void Vulkan::ImGui_DeInit()
{
    if (m_Imgui.imguiPool == VK_NULL_HANDLE) {
        return;
    }

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();

    vkDestroyDescriptorPool(m_Device->getDevice(), m_Imgui.imguiPool, nullptr);
    ImGui::DestroyContext();

    m_Imgui.imguiPool = VK_NULL_HANDLE;
}

void Vulkan::ImGui_NewFrame()
{
    ImGui_ImplSDL2_NewFrame();
    ImGui_ImplVulkan_NewFrame();
    ImGui::NewFrame();

    m_HasImgui = true;
}

void Vulkan::ImGui_EndFrame()
{
    ImGui::EndFrame();
    ImGui::Render();
}

bool Vulkan::ImGui_UploadFont()
{
    return ImGui_ImplVulkan_CreateFontsTexture();
}

void Vulkan::SetClearColor(glm::vec4 color)
{
    VkClearValue clearValue;
    clearValue.color = { color.r, color.g, color.b, color.a };

    m_RenderContext.renderpass->setClearValue(clearValue);
}

void Vulkan::SetClearDepth(float depth)
{
    VkClearValue clearValue;
    clearValue.depthStencil = { depth, 0 };

    m_RenderContext.renderpass->setDepthClearValue(clearValue);
}

void Vulkan::SetClearStencil(uint32_t stencil)
{
    // m_ClearStencil = stencil;
}

void Vulkan::CaptureFrame(std::function<void(std::vector<unsigned char>)>)
{
}