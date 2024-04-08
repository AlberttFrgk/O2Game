#include "Imgui.h"

#include <freetype/config/ftconfig.h>

#include <Imgui/imgui.h>
#include <Imgui/imgui_freetype.h>
#include <Imgui/imgui_internal.h>

#include <Graphics/NativeWindow.h>
#include <Exceptions/EstException.h>
#include <Misc/Filesystem.h>

namespace {
    bool IsImguiFrame = false;

    std::map<std::string, ImFont *> Fonts;
} // namespace

void Imgui::BeginFrame()
{
    if (IsImguiFrame)
        return;

    IsImguiFrame = true;
    Graphics::Renderer::Get()->ImGui_NewFrame();
}

void Imgui::EndFrame()
{
    if (!IsImguiFrame)
        return;

    IsImguiFrame = false;
    Graphics::Renderer::Get()->ImGui_EndFrame();
}

std::string HashFont(const char *buf, size_t size, float fontSize, const ImWchar *glyphRanges)
{
    std::string hash = std::to_string(size);
    hash += std::to_string(fontSize);

    for (int i = 0; glyphRanges[i] != 0; i++) {
        hash += std::to_string(glyphRanges[i]);
    }

    for (size_t i = 0; i < size; i++) {
        hash += std::to_string(buf[i]);
    }

    return hash;
}

ImFont *Imgui::AddFont(std::filesystem::path fontPath, float size, const ImWchar *glyphRanges)
{
    std::vector<uint8_t> buffer = Misc::Filesystem::ReadFile(fontPath);

    return AddFont((const char *)buffer.data(), buffer.size(), size, glyphRanges);
}

ImFont *Imgui::AddFont(const char *buf, size_t size, float fontSize, const ImWchar *glyphRanges)
{
    std::string hash = HashFont(buf, size, fontSize, glyphRanges);
    if (Fonts.find(hash) != Fonts.end()) {
        return Fonts[hash];
    }

    ImGuiIO &io = ImGui::GetIO();
    auto     window = Graphics::NativeWindow::Get();
    auto     renderer = Graphics::Renderer::Get();

    ImFontConfig conf;
    conf.OversampleH = conf.OversampleV = 0;
    conf.PixelSnapH = true;
    conf.SizePixels = fontSize;
    conf.GlyphOffset.y = 1.0f * (fontSize / 16.0f);
    conf.FontDataOwnedByAtlas = false;

    io.Fonts->FontBuilderIO = ImGuiFreeType::GetBuilderForFreeType();
    io.Fonts->FontBuilderFlags = ImGuiFreeTypeBuilderFlags_NoHinting;

    ImFont *font = io.Fonts->AddFontFromMemoryTTF((void *)buf, (int)size, fontSize, &conf, glyphRanges);
    if (!renderer->GetBackend()->ImGui_UploadFont()) {
        throw Exceptions::EstException("Failed to upload font to the GPU");
    }

    Fonts[hash] = font;

    return font;
}

void Imgui::TextAligment(const char *text, float alignment)
{
    ImGuiStyle &style = ImGui::GetStyle();

    float size = ImGui::CalcTextSize(text).x + style.FramePadding.x * 2.0f;
    float avail = ImGui::GetContentRegionAvail().x;

    float off = (avail - size) * alignment;
    if (off > 0.0f)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + off);

    return ImGui::Text("%s", text);
}
void Imgui::TextBackground(ImVec4 bgColor, ImVec2 size, const char *format, ...)
{
    char buf[1024];
    {
        va_list args;
        va_start(args, format);
        vsnprintf(buf, IM_ARRAYSIZE(buf), format, args);
        va_end(args);
    }

    {
        char        tmpBuf[1024];
        const char *tmpFormat = " %s ";
        snprintf(tmpBuf, IM_ARRAYSIZE(tmpBuf), tmpFormat, buf);

        strcpy(buf, tmpBuf);
    }

    ImGuiWindow *window = ImGui::GetCurrentWindow();
    ImVec2       text_pos(window->DC.CursorPos.x, window->DC.CursorPos.y + window->DC.CurrLineTextBaseOffset);

    ImVec2 text_size = ImGui::CalcTextSize(buf, NULL, true);
    if (size.x == 0) {
        size.x = text_size.x;
    }

    if (size.y == 0) {
        size.y = text_size.y;
    }

    // add 2 px padding
    size.y += 5;
    size.x += 4;

    ImGui::GetWindowDrawList()->AddRectFilled(text_pos, ImVec2(text_pos.x + size.x, text_pos.y + size.y), ImGui::GetColorU32(bgColor));
    return ImGui::Text("%s", buf);
}