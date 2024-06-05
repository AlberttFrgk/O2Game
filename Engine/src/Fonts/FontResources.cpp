#pragma warning(disable : 4838) // Goddamit
#pragma warning(disable : 4309)

#include <Misc/md5.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>

#include "Configuration.h"
#include "Fonts/FontResources.h"
#include "Rendering/Window.h"

#include "../Data/Imgui/imgui_impl_sdl2.h"
#include "../Data/Imgui/imgui_impl_sdlrenderer2.h"
#include "../Data/Imgui/imgui_impl_vulkan.h"
#include "../Data/Imgui/misc/freetype/imgui_freetype.h"
#include "Imgui/imgui.h"

#include "Rendering/Renderer.h"
#include "Rendering/Vulkan/VulkanEngine.h"

// BEGIN FONT FALLBACK

#include "FallbackFonts/arial.ttf.h"
//#include "FallbackFonts/ch.ttf.h"
//#include "FallbackFonts/jp.ttf.h"
//#include "FallbackFonts/kr.ttf.h"
#include <Logs.h>

// END FONT FALLBACK

namespace {
    int        CurrentFontIndex = 0;
    bool       gImFontRebuild = true;
    std::mutex mutex;

    FontResolution             Font;
    std::map<TextRegion, bool> gRegions = {
        { TextRegion::Chinese, false },
        { TextRegion::Japanese, false },
        { TextRegion::Korean, false },
    };
} // namespace

double calculateDisplayDPI(double diagonalDPI, double horizontalDPI, double verticalDPI)
{
    double horizontalSquared = pow(horizontalDPI, 2);
    double verticalSquared = pow(verticalDPI, 2);
    double sumSquared = horizontalSquared + verticalSquared;
    double root = sqrt(sumSquared);
    double displayDPI = root / diagonalDPI;

    return displayDPI;
}

void FontResources::RegisterFontIndex(int idx, int width, int height)
{
    FontResolution font;
    font.Index = idx;
    font.Width = width;
    font.Height = height;

    Font = font;
}

void FontResources::ClearFontIndex()
{
    // Fonts.clear();
}

void FontResources::PreloadFontCaches()
{
    Logs::Puts("[FontResources] Preparing font to load");
    std::lock_guard<std::mutex> lock(mutex);

    if (!gImFontRebuild) {
        return;
    }

    if (Font.Height == 0 || Font.Width == 0) {
        Font.Width = 800, Font.Height = 600;
        Font.Index = 0;
    }

    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    io.Fonts->Clear();
    io.Fonts->ClearFonts();

    GameWindow *wnd = GameWindow::GetInstance();

    auto skinPath = Configuration::Font_GetPath();
    auto fontPath = skinPath / "Fonts";

    auto normalfont = fontPath / "normal.ttf";
    auto jpFont = fontPath / "jp.ttf";
    auto krFont = fontPath / "kr.ttf";
    auto chFont = fontPath / "ch.ttf";

    static const ImWchar glyphRanges[] = { // Optimized
        (ImWchar)0x0020, (ImWchar)0x052F,
        (ImWchar)0x2000, (ImWchar)0x27BF,
        (ImWchar)0x2E80, (ImWchar)0x2FA1,
        (ImWchar)0x1F300, (ImWchar)0x1FAFF,
        (ImWchar)0x2660, (ImWchar)0x2663,
        (ImWchar)0x2665, (ImWchar)0x2666,
        (ImWchar)0x2600, (ImWchar)0x2606,
        (ImWchar)0x2618, (ImWchar)0x2619,
        (ImWchar)0x263A, (ImWchar)0x263B,
        (ImWchar)0x2708, (ImWchar)0x2714,
        (ImWchar)0x2728, (ImWchar)0x2734,
        (ImWchar)0x2740, (ImWchar)0x274B,
        (ImWchar)0x2756, (ImWchar)0x2758,
        (ImWchar)0x2764, (ImWchar)0x2767,
        (ImWchar)0x2794, (ImWchar)0x27BE,
        (ImWchar)0x27F0, (ImWchar)0x27FF,
        (ImWchar)0x2900, (ImWchar)0x297F,
        (ImWchar)0x2A00, (ImWchar)0x2AFF,
        (ImWchar)0x0000, (ImWchar)0x0000
    };


    {
        float originScale = (wnd->GetBufferWidth() + wnd->GetBufferHeight()) / 15.6f;
        float targetScale = (wnd->GetWidth() + wnd->GetHeight()) / 15.6f;

        float fontSize = ::round(13.0f * (targetScale / originScale));

        ImFontConfig conf;
        conf.OversampleH = conf.OversampleV = 0;
        conf.PixelSnapH = true;
        conf.SizePixels = fontSize;
        conf.GlyphOffset.y = 1.0f * (fontSize / 16.0f);
        conf.FontDataOwnedByAtlas = false;

        io.Fonts->FontBuilderIO = ImGuiFreeType::GetBuilderForFreeType();
        io.Fonts->FontBuilderFlags = ImGuiFreeTypeBuilderFlags_NoHinting;

        {
            if (std::filesystem::exists(normalfont)) {
                Font.Font = io.Fonts->AddFontFromFileTTF((const char *)normalfont.u8string().c_str(), fontSize, &conf, glyphRanges); // glyphRanges, fixing missing fonts
            } else {
                Font.Font = io.Fonts->AddFontFromMemoryTTF((void *)get_arial_font_data(), get_arial_font_size(), fontSize, &conf, glyphRanges);
            }
        }

        {
            conf.MergeMode = true;

            for (auto &[region, enabled] : gRegions) {
                if (enabled) {
                    switch (region) {
                        case TextRegion::Japanese:
                        {
                            if (std::filesystem::exists(jpFont)) {
                                io.Fonts->AddFontFromFileTTF((const char *)jpFont.u8string().c_str(), fontSize, &conf, io.Fonts->GetGlyphRangesJapanese());
                            }

                            break;
                        }

                        case TextRegion::Korean:
                        {
                            if (std::filesystem::exists(krFont)) {
                                io.Fonts->AddFontFromFileTTF((const char *)krFont.u8string().c_str(), fontSize, &conf, io.Fonts->GetGlyphRangesKorean());
                            }

                            break;
                        }

                        case TextRegion::Chinese:
                        {
                            if (std::filesystem::exists(chFont)) {
                                io.Fonts->AddFontFromFileTTF((const char *)chFont.u8string().c_str(), fontSize, &conf, io.Fonts->GetGlyphRangesChineseFull());
                            }

                            break;
                        }
                    }
                }
            }

            bool result = io.Fonts->Build();
            if (!result) {
                throw std::runtime_error("Failed to build font atlas");
            }
        }

        {
            conf.MergeMode = false;

            io.Fonts->FontBuilderIO = ImGuiFreeType::GetBuilderForFreeType();
            io.Fonts->FontBuilderFlags = ImGuiFreeTypeBuilderFlags_NoHinting;

            float iBtnFontSz = ::round(20.0f * (targetScale / originScale));
            conf.SizePixels = iBtnFontSz;
            conf.GlyphOffset.y = 1.0f * (iBtnFontSz / 16.0f);

            if (std::filesystem::exists(normalfont)) {
                Font.ButtonFont = io.Fonts->AddFontFromFileTTF((const char *)normalfont.u8string().c_str(), iBtnFontSz, &conf);
                Font.SliderFont = io.Fonts->AddFontFromFileTTF((const char *)normalfont.u8string().c_str(), iBtnFontSz, &conf);
            } else {
                Font.ButtonFont = io.Fonts->AddFontFromMemoryTTF((void *)get_arial_font_data(), get_arial_font_size(), iBtnFontSz, &conf);
                Font.SliderFont = io.Fonts->AddFontFromMemoryTTF((void *)get_arial_font_data(), get_arial_font_size(), iBtnFontSz, &conf);
            }

            io.Fonts->Build();
        }
    }

    if (Renderer::GetInstance()->IsVulkan()) {
        auto vulkan = Renderer::GetInstance()->GetVulkanEngine();

        // execute a gpu command to upload imgui font textures
        vulkan->immediate_submit([&](VkCommandBuffer cmd) {
            bool success = ImGui_ImplVulkan_CreateFontsTexture(cmd);
            if (!success) {
                throw std::runtime_error("[Vulkan] Failed to create fonts texture!");
            }
        });

        // clear font textures from cpu data
        ImGui_ImplVulkan_DestroyFontUploadObjects();
    } else {
        ImGui_ImplSDLRenderer2_DestroyDeviceObjects();
        ImGui_ImplSDLRenderer2_DestroyFontsTexture();
    }

    gImFontRebuild = false;

    Logs::Puts("[FontResources] Preload font completed!");
}

bool FontResources::ShouldRebuild()
{
    return gImFontRebuild;
}

void FontResources::DoRebuild()
{
    gImFontRebuild = true;
}

void FontResources::LoadFontRegion(TextRegion region)
{
    for (auto &[textRegion, enabled] : gRegions) {
        if (textRegion == region) {
            enabled = true;
        }
    }
}

FontResolution *FindFontIndex(int idx)
{
    return &Font;
}

void FontResources::SetFontIndex(int idx)
{
    CurrentFontIndex = idx;

    ImGui::GetIO().FontDefault = GetFont();
}

FontResolution *FontResources::GetFontIndex(int idx)
{
    return FindFontIndex(idx);
}

ImFont *FontResources::GetFont()
{
    FontResolution *font = FindFontIndex(CurrentFontIndex);

    return font ? font->Font : nullptr;
}

ImFont *FontResources::GetButtonFont()
{
    FontResolution *font = FindFontIndex(CurrentFontIndex);

    return font ? font->ButtonFont : nullptr;
}

ImFont *FontResources::GetReallyBigFontForSlider()
{
    FontResolution *font = FindFontIndex(CurrentFontIndex);

    return font ? font->SliderFont : nullptr;
}