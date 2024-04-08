#ifndef __IMGUI_H
#define __IMGUI_H

#include <iostream>
#include <Graphics/Renderer.h>
#include <Imgui/imgui.h>

namespace Imgui {
    void BeginFrame();
    void EndFrame();

    ImFont *AddFont(std::filesystem::path path, float fontSize, const ImWchar *glyphRanges = NULL);
    ImFont *AddFont(const char *buf, size_t size, float fontSize, const ImWchar *glyphRanges = NULL);

    void TextAligment(const char *text, float alignment);
    void TextBackground(ImVec4 bgColor, ImVec2 size, const char *format, ...);
} // namespace Imgui

#endif