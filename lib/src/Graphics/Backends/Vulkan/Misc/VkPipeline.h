/*
 * MIT License
 *
 * Copyright (c) 2023 Estrol Mendex
 * See the LICENSE file in the root of this project for details.
 */

#ifndef __PIPELINE_H_
#define __PIPELINE_H_

#include "../third-party/Volk/volk.h"
#include "VkShader.h"
#include "VkSwapchain.h"
#include <Graphics/GraphicsBackendBase.h>

class VK_Pipeline
{
public:
    VK_Pipeline() = default;
    VK_Pipeline(
        VK_Device                           *device,
        VkPipelineLayout                     layout,
        VkRenderPass                         renderpass,
        VK_Shader                           *shader,
        std::string                          EntryPoint,
        Graphics::Backends::TextureBlendInfo info);
    ~VK_Pipeline();

    void Bind(VkCommandBuffer cmd);

private:
    VkPipeline m_Pipeline;

    VK_Device    *m_Device = nullptr;
    VK_Swapchain *m_Swapchain = nullptr;
};

#endif