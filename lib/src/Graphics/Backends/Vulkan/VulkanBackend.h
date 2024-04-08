/*
 * MIT License
 *
 * Copyright (c) 2023 Estrol Mendex
 * See the LICENSE file in the root of this project for details.
 */

#ifndef __VULKANBACKEND_H_
#define __VULKANBACKEND_H_

#include <deque>
#include <functional>
#include <memory>
#include <unordered_map>

#include "./Misc/VkCommandPool.h"
#include "./Misc/VkDevice.h"
#include "./Misc/VkMemory.h"
#include "./Misc/VkPipeline.h"
#include "./Misc/VkRenderchain.h"
#include "./Misc/VkRenderpass.h"
#include "./Misc/VkShader.h"
#include "./Misc/VkSwapchain.h"

#include "VulkanDescriptor.h"
#include <Graphics/GraphicsBackendBase.h>
#include <Graphics/GraphicsTexture2D.h>

struct DeletionQueue
{
    std::deque<std::function<void()>> deletors;

    void push_function(std::function<void()> &&function)
    {
        deletors.push_back(function);
    }

    void flush()
    {
        for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
            (*it)();
        }

        deletors.clear();
    }
};

namespace Graphics {
    namespace Backends {
        struct VulkanSync
        {
            VkSemaphore renderSemaphore;
            VkSemaphore presentSemaphore;
            VkFence     renderFence;
        };

        struct VulkanDescriptorPool
        {
            VkDescriptorSetLayout layout;
            VkDescriptorPool      pool;
        };

        struct VulkanPipeline
        {
            VkDescriptorSetLayout layout;
            VkPipelineLayout      pipelineLayout;
        };

        struct VulkanFrame
        {
            VkCommandBuffer commandBuffer;
            VulkanSync      sync;
        };

        struct VulkanRenderContext
        {
            std::shared_ptr<VK_Renderpass> renderpass;
            VkExtent3D                     extent;
        };

        struct VulkanRenderBuffer
        {
            std::shared_ptr<VK_Memory> VertexBuffer;
            std::shared_ptr<VK_Memory> IndexBuffer;

            uint32_t maxVertexBufferSize;
            uint32_t maxIndexBufferSize;
        };

        struct VulkanImGui
        {
            VkDescriptorPool imguiPool;
        };

        struct VulkanDrawData
        {
            Vertex   *vertexPtr;
            uint16_t *indexPtr;

            uint32_t vertexSize;
            uint32_t indiceSize;

            void Reset()
            {
                vertexSize = 0;
                indiceSize = 0;
            }
        };

        struct VulkanDrawItem
        {
            uint32_t       count;
            uint16_t       instanceCount;
            PipelineHandle pipeline;
            const void    *image;

            Rect      clipRect;
            glm::vec2 uiSize;
            glm::vec4 uiRadius;
        };

        struct VulkanSampler
        {
            std::string hash;
            VkSampler   sampler;
        };

        class Vulkan : public Base
        {
        public:
            virtual ~Vulkan() = default;

            virtual void Init() override;
            virtual void ReInit() override;
            virtual void Shutdown() override;

            virtual bool NeedReinit() override;

            virtual bool BeginFrame() override;
            virtual void EndFrame() override;

            virtual void ImGui_Init() override;
            virtual void ImGui_DeInit() override;
            virtual void ImGui_NewFrame() override;
            virtual void ImGui_EndFrame() override;
            virtual bool ImGui_UploadFont() override;

            virtual void Push(SubmitInfo &info) override;
            virtual void Push(std::vector<SubmitInfo> &infos) override;

            virtual void SetVSync(bool enabled) override;
            virtual void SetClearColor(glm::vec4 color) override;
            virtual void SetClearDepth(float depth) override;
            virtual void SetClearStencil(uint32_t stencil) override;

            virtual PipelineHandle CreatePipeline(PipelineInfo &info) override;
            VkSampler              CreateSampler(Graphics::TextureSamplerInfo info);

            virtual void CaptureFrame(std::function<void(std::vector<unsigned char>)>) override;

            /* Internal */
            VulkanDescriptor *CreateDescriptor();
            void              DestroyDescriptor(VulkanDescriptor *descriptor, bool _delete = true);

            VK_Device            *GetDevice();
            VK_Swapchain         *GetSwapchain();
            VulkanDescriptorPool *GetDescriptorPool();

            void ImmediateSubmit(std::function<void(VkCommandBuffer)> &&function);
            void AsyncSubmit(std::function<void(VkCommandBuffer)> &&function);

        private:
            void CreateInstance();
            void CreateRenderpass();
            void InitCommands();
            void InitSyncStructures();
            void InitDescriptors();
            void InitShaders();
            void InitPipeline();
            bool InitSwapchain();

            void         FlushQueue();
            void         ResizeBuffer(VkDeviceSize vertices, VkDeviceSize indices);
            void         ImageBarrier(VkCommandBuffer cmdbuffer,
                                      VkImage image, VkImageLayout oldLayout,
                                      VkImageLayout newLayout, VkImageAspectFlags aspectMask);
            VulkanFrame &GetCurrentFrame();
            VulkanFrame &GetLastFrame();

            std::shared_ptr<VK_Device>      m_Device;
            std::shared_ptr<VK_Swapchain>   m_Swapchain;
            std::shared_ptr<VK_Renderchain> m_Renderchain;
            std::vector<VulkanFrame>        m_Frames;
            std::shared_ptr<VK_CommandPool> m_CommandPool;

            VulkanImGui          m_Imgui;
            VulkanDescriptorPool m_Descriptor;
            VulkanPipeline       m_Pipeline;
            VulkanRenderContext  m_RenderContext;
            VulkanRenderContext  m_PresentContext;
            VulkanRenderBuffer   m_RenderBuffer;
            VulkanDrawData       m_RenderDrawData;

            // Pending infos
            std::vector<VulkanDrawItem> submitInfos;

            // OnExit program clean up
            DeletionQueue m_DeletionQueue;

            // OnFrame program clean up, like deleting texture
            DeletionQueue m_PerFrameDeletionQueue;

            // OnSwapchain program clean up, like re-creating swapchain
            DeletionQueue m_SwapchainDeletionQueue;

            bool m_SwapchainReady;
            bool m_Initialized;
            bool m_FrameBegin;
            bool m_HasImgui = false;
            bool m_VSync = false;
            bool m_FenceRequireReset = false;
            bool m_BothRenderAndPresent = false;

            uint32_t m_CurrentFrame = 0;
            uint32_t m_SwapchainIndex = 0;
            uint32_t m_FrameWithoutSwapchain = 0;

            // Descriptor, for auto cleanup
            std::vector<std::unique_ptr<VulkanDescriptor>> m_Descriptors;
            uint32_t                                       m_DescriptorId = 0;

            // Submit Queues or Deferred Queues
            std::vector<SubmitInfo> m_SubmitInfos;

            // Sampler Cache
            std::unordered_map<std::string, VulkanSampler> m_SamplerCache;

            // Pipeline Cache
            std::unordered_map<std::string, PipelineHandle>                  m_PipelineHash;
            std::unordered_map<PipelineHandle, std::shared_ptr<VK_Pipeline>> m_PipelineCache;

            PipelineHandle m_FallbackPipeline;
        };
    } // namespace Backends
} // namespace Graphics

#endif