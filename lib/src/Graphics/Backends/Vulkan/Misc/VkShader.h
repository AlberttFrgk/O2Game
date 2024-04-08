/*
 * MIT License
 *
 * Copyright (c) 2023 Estrol Mendex
 * See the LICENSE file in the root of this project for details.
 */

#ifndef __SHADER_H_
#define __SHADER_H_

#include "VkDevice.h"
#include <string>

class VK_Shader
{
public:
    VK_Shader() = default;
    VK_Shader(VK_Device *device, const std::string vert = "", const std::string frag = "");
    VK_Shader(VK_Device *device, const std::vector<uint32_t> vert, const std::vector<uint32_t> frag);
    VK_Shader(VK_Device *device, const uint32_t *vert, const uint32_t *frag, size_t vertSize, size_t fragSize);

    ~VK_Shader();

    VkShaderModule getVertexShader();
    VkShaderModule getFragmentShader();

private:
    VK_Device     *vkdevice = nullptr;
    VkShaderModule vertexShader;
    VkShaderModule fragmentShader;
};

#endif // __SHADER_H_