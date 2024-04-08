/*
 * MIT License
 *
 * Copyright (c) 2023 Estrol Mendex
 * See the LICENSE file in the root of this project for details.
 */

#include "VkPipeline.h"
#include <Exceptions/EstException.h>
#include <Graphics/GraphicsBackendBase.h>

VK_Pipeline::VK_Pipeline(
    VK_Device                           *device,
    VkPipelineLayout                     layout,
    VkRenderPass                         renderpass,
    VK_Shader                           *shader,
    std::string                          EntryPoint,
    Graphics::Backends::TextureBlendInfo info)
{
    VkPipelineShaderStageCreateInfo stage[2] = {};
    stage[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stage[0].module = shader->getVertexShader();
    stage[0].pName = EntryPoint.c_str();
    stage[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stage[1].module = shader->getFragmentShader();
    stage[1].pName = EntryPoint.c_str();

    VkVertexInputBindingDescription binding_desc[1] = {};
    binding_desc[0].stride = sizeof(Graphics::Backends::Vertex);
    binding_desc[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attribute_desc[3] = {};
    attribute_desc[0].location = 0;
    attribute_desc[0].binding = binding_desc[0].binding;
    attribute_desc[0].format = VK_FORMAT_R32G32_SFLOAT;
    attribute_desc[0].offset = MY_OFFSETOF(Graphics::Backends::Vertex, pos);
    attribute_desc[1].location = 1;
    attribute_desc[1].binding = binding_desc[0].binding;
    attribute_desc[1].format = VK_FORMAT_R32G32_SFLOAT;
    attribute_desc[1].offset = MY_OFFSETOF(Graphics::Backends::Vertex, texCoord);
    attribute_desc[2].location = 2;
    attribute_desc[2].binding = binding_desc[0].binding;
    attribute_desc[2].format = VK_FORMAT_R8G8B8A8_UNORM;
    attribute_desc[2].offset = MY_OFFSETOF(Graphics::Backends::Vertex, color);

    VkPipelineVertexInputStateCreateInfo vertex_info = {};
    vertex_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_info.vertexBindingDescriptionCount = 1;
    vertex_info.pVertexBindingDescriptions = binding_desc;
    vertex_info.vertexAttributeDescriptionCount = sizeof(attribute_desc) / sizeof(attribute_desc[0]);
    vertex_info.pVertexAttributeDescriptions = attribute_desc;

    VkPipelineInputAssemblyStateCreateInfo ia_info = {};
    ia_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    ia_info.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewport_info = {};
    viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_info.viewportCount = 1;
    viewport_info.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo raster_info = {};
    raster_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster_info.polygonMode = VK_POLYGON_MODE_FILL;
    raster_info.cullMode = VK_CULL_MODE_NONE;
    raster_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster_info.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms_info = {};
    ms_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depth_info = {};
    depth_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

    VkPipelineColorBlendStateCreateInfo blend_info = {};
    blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend_info.attachmentCount = 1;

    VkDynamicState                   dynamic_states[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamic_state = {};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = (uint32_t)(sizeof(dynamic_states) / sizeof(dynamic_states[0]));
    dynamic_state.pDynamicStates = dynamic_states;

    VkPipelineColorBlendAttachmentState color_attachment[1] = {};
    if (info.Enable) {
        color_attachment[0].blendEnable = VK_TRUE;
        color_attachment[0].srcColorBlendFactor = static_cast<VkBlendFactor>(info.SrcColor);
        color_attachment[0].dstColorBlendFactor = static_cast<VkBlendFactor>(info.DstColor);
        color_attachment[0].colorBlendOp = static_cast<VkBlendOp>(info.ColorOp);
        color_attachment[0].srcAlphaBlendFactor = static_cast<VkBlendFactor>(info.SrcAlpha);
        color_attachment[0].dstAlphaBlendFactor = static_cast<VkBlendFactor>(info.DstAlpha);
        color_attachment[0].alphaBlendOp = static_cast<VkBlendOp>(info.AlphaOp);
        color_attachment[0].colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    } else {
        color_attachment[0].blendEnable = VK_FALSE;
    }

    blend_info.pAttachments = color_attachment;

    VkGraphicsPipelineCreateInfo ginfo = {};
    ginfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    ginfo.flags = 0;
    ginfo.stageCount = 2;
    ginfo.pStages = stage;
    ginfo.pVertexInputState = &vertex_info;
    ginfo.pInputAssemblyState = &ia_info;
    ginfo.pViewportState = &viewport_info;
    ginfo.pRasterizationState = &raster_info;
    ginfo.pMultisampleState = &ms_info;
    ginfo.pDepthStencilState = &depth_info;
    ginfo.pColorBlendState = &blend_info;
    ginfo.pDynamicState = &dynamic_state;
    ginfo.layout = layout;
    ginfo.renderPass = renderpass;
    ginfo.subpass = 0;

    VkResult result = vkCreateGraphicsPipelines(device->getDevice(), VK_NULL_HANDLE, 1, &ginfo, nullptr, &m_Pipeline);

    if (result != VK_SUCCESS) {
        throw Exceptions::EstException("Failed to create graphics pipeline");
    }
}

VK_Pipeline::~VK_Pipeline()
{
    if (m_Device) {
        vkDestroyPipeline(m_Device->getDevice(), m_Pipeline, nullptr);
    }
}

void VK_Pipeline::Bind(VkCommandBuffer cmd)
{
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline);
}