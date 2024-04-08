/*
 * MIT License
 *
 * Copyright (c) 2023 Estrol Mendex
 * See the LICENSE file in the root of this project for details.
 */

#ifndef __PIPELINE_BLEND_H_
#define __PIPELINE_BLEND_H_

#include <Graphics/GraphicsBackendBase.h>

Graphics::Backends::TextureBlendInfo blendNone = {
    true,
    Graphics::Backends::BlendFactor::BLEND_FACTOR_ONE,
    Graphics::Backends::BlendFactor::BLEND_FACTOR_ZERO,
    Graphics::Backends::BlendOp::BLEND_OP_ADD,
    Graphics::Backends::BlendFactor::BLEND_FACTOR_ONE,
    Graphics::Backends::BlendFactor::BLEND_FACTOR_ZERO,
    Graphics::Backends::BlendOp::BLEND_OP_ADD
};

Graphics::Backends::TextureBlendInfo blendBlend = {
    true,
    Graphics::Backends::BlendFactor::BLEND_FACTOR_SRC_ALPHA,
    Graphics::Backends::BlendFactor::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    Graphics::Backends::BlendOp::BLEND_OP_ADD,
    Graphics::Backends::BlendFactor::BLEND_FACTOR_SRC_ALPHA,
    Graphics::Backends::BlendFactor::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    Graphics::Backends::BlendOp::BLEND_OP_ADD
};

Graphics::Backends::TextureBlendInfo blendAdd = {
    true,
    Graphics::Backends::BlendFactor::BLEND_FACTOR_ONE,
    Graphics::Backends::BlendFactor::BLEND_FACTOR_ONE,
    Graphics::Backends::BlendOp::BLEND_OP_ADD,
    Graphics::Backends::BlendFactor::BLEND_FACTOR_ONE,
    Graphics::Backends::BlendFactor::BLEND_FACTOR_ONE,
    Graphics::Backends::BlendOp::BLEND_OP_ADD
};

Graphics::Backends::TextureBlendInfo blendMod = {
    true,
    Graphics::Backends::BlendFactor::BLEND_FACTOR_DST_COLOR,
    Graphics::Backends::BlendFactor::BLEND_FACTOR_ZERO,
    Graphics::Backends::BlendOp::BLEND_OP_ADD,
    Graphics::Backends::BlendFactor::BLEND_FACTOR_ONE,
    Graphics::Backends::BlendFactor::BLEND_FACTOR_ZERO,
    Graphics::Backends::BlendOp::BLEND_OP_ADD
};

Graphics::Backends::TextureBlendInfo blendMul = {
    true,
    Graphics::Backends::BlendFactor::BLEND_FACTOR_SRC_COLOR,
    Graphics::Backends::BlendFactor::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    Graphics::Backends::BlendOp::BLEND_OP_ADD,
    Graphics::Backends::BlendFactor::BLEND_FACTOR_SRC_ALPHA,
    Graphics::Backends::BlendFactor::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    Graphics::Backends::BlendOp::BLEND_OP_ADD
};

#endif