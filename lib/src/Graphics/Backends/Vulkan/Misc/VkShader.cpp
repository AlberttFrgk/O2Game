/*
 * MIT License
 *
 * Copyright (c) 2023 Estrol Mendex
 * See the LICENSE file in the root of this project for details.
 */

#include "VkShader.h"
#include <Exceptions/EstException.h>

#include <filesystem>
#include <fstream>

VkShaderModule createShaderModule(VK_Device *device, const std::vector<uint32_t> &code)
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size() * sizeof(uint32_t);
    createInfo.pCode = code.data();

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device->getDevice(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw Exceptions::EstException("Failed to create shader module");
    }

    return shaderModule;
}

VK_Shader::VK_Shader(VK_Device *device, const std::string vert, const std::string frag)
{
    this->vkdevice = device;

    std::vector<uint32_t> vertCode;
    std::vector<uint32_t> fragCode;

    std::fstream vertFile(vert, std::ios::in | std::ios::binary);
    if (!vertFile.is_open()) {
        throw Exceptions::EstException("Failed to open vertex shader file");
    }

    std::fstream fragFile(frag, std::ios::in | std::ios::binary);
    if (!fragFile.is_open()) {
        throw Exceptions::EstException("Failed to open fragment shader file");
    }

    vertFile.seekg(0, std::ios::end);
    vertCode.resize(vertFile.tellg() / sizeof(uint32_t));
    vertFile.seekg(0, std::ios::beg);

    fragFile.seekg(0, std::ios::end);
    fragCode.resize(fragFile.tellg() / sizeof(uint32_t));
    fragFile.seekg(0, std::ios::beg);

    vertFile.read(reinterpret_cast<char *>(vertCode.data()), vertCode.size() * sizeof(uint32_t));
    fragFile.read(reinterpret_cast<char *>(fragCode.data()), fragCode.size() * sizeof(uint32_t));

    vertFile.close();
    fragFile.close();

    vertexShader = createShaderModule(device, vertCode);
    fragmentShader = createShaderModule(device, fragCode);
}

VK_Shader::VK_Shader(VK_Device *device, const std::vector<uint32_t> vert, const std::vector<uint32_t> frag)
{
    this->vkdevice = device;

    vertexShader = createShaderModule(device, vert);
    fragmentShader = createShaderModule(device, frag);
}

VK_Shader::VK_Shader(VK_Device *device, const uint32_t *vert, const uint32_t *frag, size_t vertSize, size_t fragSize)
{
    this->vkdevice = device;

    std::vector<uint32_t> vertCode(vert, vert + vertSize);
    std::vector<uint32_t> fragCode(frag, frag + fragSize);

    vertexShader = createShaderModule(device, vertCode);
    fragmentShader = createShaderModule(device, fragCode);
}

VK_Shader::~VK_Shader()
{
    vkDestroyShaderModule(vkdevice->getDevice(), vertexShader, nullptr);
    vkDestroyShaderModule(vkdevice->getDevice(), fragmentShader, nullptr);
}

VkShaderModule VK_Shader::getVertexShader()
{
    return vertexShader;
}

VkShaderModule VK_Shader::getFragmentShader()
{
    return fragmentShader;
}