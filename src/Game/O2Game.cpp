/*
 * MIT License
 *
 * Copyright (c) 2023 Estrol Mendex
 * See the LICENSE file in the root of this project for details.
 */

#include "O2Game.h"
#include <Imgui/imgui.h>
#include <Screens/ScreenManager.h>
#include <Configuration.h>

#include <Audio/AudioEngine.h>
#include <Audio/AudioSample.h>

#include "../Scenes/Gameplay.h"
#include "../Scenes/Loading.h"
#include "../Scenes/Result.h"
#include "../Scenes/SongSelect.h"

#include "../Scenes/SceneList.h"
#include "../Game/Core/Enums/FrameRate.h"

#include "Core/Skinning/LuaSkin.h"
#include "Core/Skinning/LuaManager.h"

#include "DefaultConfiguration.h"
#include "Env.h"
#include "MsgBoxEx.h"

#include "../../lib/src/Graphics/Shaders/SPV/image.spv.h"
#include "../../lib/src/Graphics/Shaders/SPV/position.spv.h"

#include "./Core/Imgui/Imgui.h"

O2Game::O2Game()
{
}

O2Game::~O2Game()
{
}

void O2Game::Run(int argv, char **argc)
{
    DefaultConfiguration::CreateDefault();

    RunInfo runInfo = {};
#if defined(_DEBUG)
    runInfo.title = "O2Game (Debug)";
#else
    runInfo.title = "O2Game";
#endif

    runInfo.resolution = {
        Configuration::GetFloat("Graphics", "Width", 800.0f),
        Configuration::GetFloat("Graphics", "Height", 600.0f)
    };

    auto manager = LuaManager::Get();
    manager->LoadSkin("Default");

    std::string widthstring = manager->GetSkinProp("Width", "800");
    std::string heightstring = manager->GetSkinProp("Height", "600");
    float       width = 800.0f;
    float       height = 600.0f;

    try {
        width = std::stof(widthstring);
        height = std::stof(heightstring);
    } catch (std::exception &e) {
        std::cerr << "Failed to convert width and height to float: " << e.what() << std::endl;
    }

    runInfo.buffer_resolution = {
        width, height
    };

    int renderer = Configuration::GetInt("Graphics", "Renderer", 0);
    switch (renderer) {
        case 0:
            runInfo.graphics = Graphics::API::Vulkan;
            break;
        default:
            runInfo.graphics = Graphics::API::Vulkan;
            break;
    }

    // runInfo.threadMode = ThreadMode::Single;
    int renderthread = Configuration::GetInt("Graphics", "RenderThread", 1);
    if (renderthread == 1) {
        runInfo.threadMode = ThreadMode::Multi;
    } else {
        runInfo.threadMode = ThreadMode::Single;
    }

    runInfo.renderVsync = false;

    float           renderFrameRate = 1000.0f;
    FrameRate::Mode frameMode = static_cast<FrameRate::Mode>(Configuration::GetInt("Graphics", "FrameMode", 0));
    switch (frameMode) {
        case FrameRate::Mode::Optimal:
        {
            renderFrameRate = 240.0f;
            break;
        }

        case FrameRate::Mode::VSync:
        {
            renderFrameRate = 480.0f;
            runInfo.renderVsync = true;
            break;
        }

        case FrameRate::Mode::Fixed:
        {
            float fixedFrameRate = Configuration::GetFloat("Graphics", "FrameRate", 120.0f);
            renderFrameRate = fixedFrameRate;
            break;
        }

        case FrameRate::Mode::Unlimited:
        {
            renderFrameRate = 1000.0f;
            break;
        }
    }

    runInfo.inputFrameRate = 1000.0f;
    runInfo.fixedFrameRate = 65.0f;

    // Don't change this, as this will cause texture to blurry when scaling
    Graphics::TextureSamplerInfo sampler = {};
    sampler.FilterMag = Graphics::TextureFilter::Nearest;
    sampler.FilterMin = Graphics::TextureFilter::Nearest;
    sampler.AddressModeU = Graphics::TextureAddressMode::Repeat;
    sampler.AddressModeV = Graphics::TextureAddressMode::Repeat;
    sampler.AddressModeW = Graphics::TextureAddressMode::Repeat;
    sampler.MipLodBias = 0.0f;
    sampler.AnisotropyEnable = false;
    sampler.MaxAnisotropy = 0.0f;
    sampler.CompareEnable = false;
    sampler.CompareOp = Graphics::TextureCompareOP::COMPARE_OP_ALWAYS;
    sampler.MinLod = 0.0f;
    sampler.MaxLod = Graphics::kMaxLOD;

    runInfo.samplerInfo = sampler;

    Game::Run(runInfo);
}

void O2Game::OnLoad()
{
    using namespace Graphics::Backends;
    Game::OnLoad();

    auto renderer = Graphics::Renderer::Get();

    auto manager = LuaManager::Get();
    auto skin = manager->LoadScript(SkinGroup::Audio);
    auto audios = skin->GetAudio();
    auto audioManager = Audio::Engine::Get();

    for (auto &audio : audios) {
        Audio::Sample *sample = audioManager->LoadSample(audio.Path);
        std::string    name = AudioTypeString[audio.Type];

        Env::SetPointer(name, sample);
    }

    TextureBlendInfo blendMul = {
        true,
        BlendFactor::BLEND_FACTOR_SRC_ALPHA,
        BlendFactor::BLEND_FACTOR_ONE,
        BlendOp::BLEND_OP_ADD,
        BlendFactor::BLEND_FACTOR_ONE,
        BlendFactor::BLEND_FACTOR_ZERO,
        BlendOp::BLEND_OP_ADD
    };

    TextureBlendInfo nonBlendMul = {
        .Enable = true,
        .SrcColor = BlendFactor::BLEND_FACTOR_SRC_ALPHA,
        .DstColor = BlendFactor::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .ColorOp = BlendOp::BLEND_OP_ADD,
        .SrcAlpha = BlendFactor::BLEND_FACTOR_ONE,
        .DstAlpha = BlendFactor::BLEND_FACTOR_ZERO,
        .AlphaOp = BlendOp::BLEND_OP_ADD
    };

    // clang-format off
    PipelineInfo info = {
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
        .EntryPoint = "main"
    };
    // clang-format on

    info.BlendInfo = nonBlendMul;
    Env::SetInt("BlendNonAlpha", renderer->CreatePipeline(info));

    info.BlendInfo = blendMul;
    Env::SetInt("BlendAlpha", renderer->CreatePipeline(info));

    auto scenemanager = Screens::Manager::Get();
    scenemanager->Add<Loading>();
    scenemanager->Add<SongSelect>();
    scenemanager->Add<Gameplay>();
    scenemanager->Add<Result>();

    scenemanager->Set<Loading>();
}

void O2Game::OnUnload()
{
    Game::OnUnload();
}

void O2Game::OnInput(double delta)
{
    Game::OnInput(delta);
}

void O2Game::OnUpdate(double delta)
{
    Game::OnUpdate(delta);
}

void O2Game::OnDraw(double delta)
{
    Imgui::BeginFrame();
    Game::OnDraw(delta);
    MsgBox::Draw(delta);
    Imgui::EndFrame();
}