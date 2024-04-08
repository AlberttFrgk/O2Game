/*
 * MIT License
 *
 * Copyright (c) 2023 Estrol Mendex
 * See the LICENSE file in the root of this project for details.
 */

#include <Graphics/NativeWindow.h>
#include <Graphics/Renderer.h>
#include <UI/Text.h>
#include <cstdarg>
#include <sstream>

#include "../Graphics/Shaders/SPV/position.spv.h"
#include "../Graphics/Shaders/SPV/sdf_text.spv.h"
#include "../Graphics/Shaders/SPV/text.spv.h"

using namespace UI;

namespace {
    Graphics::Backends::PipelineHandle SDFText_Pipeline = 0;
    Graphics::Backends::PipelineHandle Text_Pipeline = 0;

    std::once_flag m_flag;
} // namespace

void InitShader()
{
    std::call_once(m_flag, []() {
        // clang-format off
        Graphics::Backends::TextureBlendInfo blendAdd = {
            true,
            Graphics::Backends::BlendFactor::BLEND_FACTOR_ONE,
            Graphics::Backends::BlendFactor::BLEND_FACTOR_ONE,
            Graphics::Backends::BlendOp::BLEND_OP_ADD,
            Graphics::Backends::BlendFactor::BLEND_FACTOR_ONE,
            Graphics::Backends::BlendFactor::BLEND_FACTOR_ONE,
            Graphics::Backends::BlendOp::BLEND_OP_ADD
        };

        Graphics::Backends::PipelineInfo info = {
            .IsFile = false,
            .VertexShader = {
                .Memory = {
                    .Code = __glsl_position,
                    .CodeSize = sizeof(__glsl_position) / sizeof(__glsl_position[0])
                }
            },
            .EntryPoint = "main",
            .BlendInfo = blendAdd
        };

        info.FragmentShader = {
            .Memory = {
                .Code = __glsl_text,
                .CodeSize = sizeof(__glsl_text) / sizeof(__glsl_text[0])
            }
        };

        Text_Pipeline = Graphics::Renderer::Get()->CreatePipeline(info);

        info.FragmentShader = {
            .Memory = {
                .Code = __glsl_sdf_text,
                .CodeSize = sizeof(__glsl_sdf_text) / sizeof(__glsl_sdf_text[0])
            }
        };

        SDFText_Pipeline = Graphics::Renderer::Get()->CreatePipeline(info);
        // clang-format on
    });
}

Text::Text() : UI::Base()
{
    Scale = 1.0f;
    TextClipping = Graphics::NativeWindow::Get()->GetBufferSize();

    m_FontAtlas = Fonts::FontManager::Get()->LoadFirst();
    if (!m_FontAtlas) {
        Fonts::FontLoadFileInfo info = {};
        info.Path = "arial.ttf";
        info.FontSize = 20.0f;

        info.Ranges.push_back({ 0x0020, 0x00FF });

        m_FontAtlas = Fonts::FontManager::Get()->LoadFont(info);
    }

    m_renderMode = RenderMode::Batches;

    m_texturePtr = m_FontAtlas->Texture.get();
    Alignment = Alignment::Left;
    IsSDF = false;

    InitShader();
}

Text::Text(std::string fontName, float fontSize) : UI::Base()
{
    Scale = 1.0f;
    TextClipping = Graphics::NativeWindow::Get()->GetBufferSize();

    Fonts::FontLoadFileInfo info = {};
    info.Path = fontName;
    info.FontSize = fontSize;

    info.Ranges.push_back({ 0x0020, 0x00FF });

    m_FontAtlas = Fonts::FontManager::Get()->LoadFont(info);
    m_renderMode = RenderMode::Batches;

    m_texturePtr = m_FontAtlas->Texture.get();
    Alignment = Alignment::Left;
    IsSDF = false;

    InitShader();
}

Text::Text(Fonts::FontLoadFileInfo &info) : UI::Base()
{
    Scale = 1.0f;
    TextClipping = Graphics::NativeWindow::Get()->GetBufferSize();

    m_FontAtlas = Fonts::FontManager::Get()->LoadFont(info);
    m_renderMode = RenderMode::Batches;

    m_texturePtr = m_FontAtlas->Texture.get();
    Alignment = Alignment::Left;
    IsSDF = false;

    InitShader();
}

Text::Text(Fonts::FontLoadBufferInfo &info) : UI::Base()
{
    Scale = 1.0f;
    TextClipping = Graphics::NativeWindow::Get()->GetBufferSize();

    m_FontAtlas = Fonts::FontManager::Get()->LoadFont(info);
    m_renderMode = RenderMode::Batches;

    m_texturePtr = m_FontAtlas->Texture.get();
    Alignment = Alignment::Left;
    IsSDF = false;

    InitShader();
}

Text::Text(Fonts::FontLoadAtlasInfo &info) : UI::Base()
{
    Scale = 1.0f;
    TextClipping = Graphics::NativeWindow::Get()->GetBufferSize();

    m_FontAtlas = Fonts::FontManager::Get()->LoadFont(info);
    m_renderMode = RenderMode::Batches;

    m_texturePtr = m_FontAtlas->Texture.get();
    Alignment = Alignment::Left;
    IsSDF = false;

    InitShader();
}

void Text::DrawString(std::string text)
{
    m_TextToDraw = text;
    Draw();
}

void Text::DrawStringFormatted(std::string text, ...)
{
    char    buf[1024];
    va_list args;
    va_start(args, text);
    vsprintf_s(buf, text.c_str(), args);
    va_end(args);

    m_TextToDraw = buf;
    Draw();
}

std::vector<std::string> split(std::string text, char delimiter)
{
    std::vector<std::string> tokens;
    std::string              token;
    std::istringstream       tokenStream(text);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

std::string trim(std::string &text)
{
    text.erase(text.begin(), std::find_if(text.begin(), text.end(), [](unsigned char ch) {
                   return !std::isspace(ch);
               }));

    text.erase(std::find_if(text.rbegin(), text.rend(), [](unsigned char ch) {
                   return !std::isspace(ch);
               }).base(),
               text.end());

    return text;
}

// Taken from Imgui
// Which modified version from: https://github.com/skeeto/branchless-utf8
#define UNICODE_CODEPOINT_MAX 0xFFFF
int charFromUTF8(unsigned int *out_char, const char *in_text, const char *in_text_end)
{
    static const char     lengths[32] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 3, 3, 4, 0 };
    static const int      masks[] = { 0x00, 0x7f, 0x1f, 0x0f, 0x07 };
    static const uint32_t mins[] = { 0x400000, 0, 0x80, 0x800, 0x10000 };
    static const int      shiftc[] = { 0, 18, 12, 6, 0 };
    static const int      shifte[] = { 0, 6, 4, 2, 0 };
    int                   len = lengths[*(const unsigned char *)in_text >> 3];
    int                   wanted = len + (len ? 0 : 1);

    if (in_text_end == NULL)
        in_text_end = in_text + wanted;

    unsigned char s[4];
    s[0] = in_text + 0 < in_text_end ? in_text[0] : 0;
    s[1] = in_text + 1 < in_text_end ? in_text[1] : 0;
    s[2] = in_text + 2 < in_text_end ? in_text[2] : 0;
    s[3] = in_text + 3 < in_text_end ? in_text[3] : 0;

    *out_char = (uint32_t)(s[0] & masks[len]) << 18;
    *out_char |= (uint32_t)(s[1] & 0x3f) << 12;
    *out_char |= (uint32_t)(s[2] & 0x3f) << 6;
    *out_char |= (uint32_t)(s[3] & 0x3f) << 0;
    *out_char >>= shiftc[len];

    int e = 0;
    e = (*out_char < mins[len]) << 6;
    e |= ((*out_char >> 11) == 0x1b) << 7;
    e |= (*out_char > UNICODE_CODEPOINT_MAX) << 8;
    e |= (s[1] & 0xc0) >> 2;
    e |= (s[2] & 0xc0) >> 4;
    e |= (s[3]) >> 6;
    e ^= 0x2a;
    e >>= shifte[len];

    if (e) {
        *out_char = '?';
    }

    return wanted;
}

void Text::OnDraw()
{
    using namespace Graphics::Backends;
    CalculateSize();
    m_batches.clear();

    // We only need to calculate the position once
    double x1 = AbsolutePosition.X;
    double y1 = AbsolutePosition.Y;

    clipRect = {
        static_cast<int>(TextClipping.X),
        static_cast<int>(TextClipping.Y),
        static_cast<int>((TextClipping.X + TextClipping.Width)),
        static_cast<int>((TextClipping.Y + TextClipping.Height))
    };

    m_ScaleSize = false;

    glm::vec4 color = {
        Color3.R * 255,
        Color3.G * 255,
        Color3.B * 255,
        Transparency * 255
    };

    glm::vec4 corRad = {
        1.0f, 1.0f, 1.0f, 1.0f
    };

    if (IsSDF) {
        PipelineHandle = SDFText_Pipeline;
    } else {
        PipelineHandle = Text_Pipeline;
    }

    uint32_t col = ((uint32_t)(color.a) << 24) | ((uint32_t)(color.b) << 16) | ((uint32_t)(color.g) << 8) | ((uint32_t)(color.r) << 0);
    float    posy = (float)y1;
    float    scale = (m_FontAtlas->FontSize * Scale) / m_FontAtlas->FontSize;

    m_SubmitInfo.indices = { 0, 1, 2, 3, 4, 5 };

    auto strings = split(m_TextToDraw, '\n');
    for (auto &string : strings) {
        float stringlength = MeasureString(trim(string)).x;
        float pos = (float)x1;

        switch (Alignment) {
            case Alignment::Center:
                pos -= stringlength / 2;
                break;
            case Alignment::Right:
                pos -= stringlength;
                break;
            default:
                break;
        }

        auto it = string.c_str();
        auto end = string.c_str() + string.size();

        while (it < end) {
            uint32_t character = *it;

            if (character >= 0x80) {
                it += charFromUTF8(&character, it, end);
            } else {
                it++;
            }

            if (character == '\r')
                continue;

            Fonts::Glyph *glyph = FindGlyph(character);

            float width = m_FontAtlas->TexSize.x;
            float height = m_FontAtlas->TexSize.y;

            float _x1 = pos + glyph->Rect.x * scale;
            float _x2 = pos + glyph->Rect.z * scale;
            float _y1 = posy + glyph->Rect.y + glyph->Ascender * scale;
            float _y2 = posy + glyph->Rect.w + glyph->Ascender * scale;

            auto &rect = clipRect;
            bool  isWithinRect = (_x1 >= rect.X && _x2 <= rect.Y + rect.Width) && (_y1 >= rect.Y && _y2 <= rect.Y + rect.Height);

            if (isWithinRect) {
                auto &uv1 = glyph->UV[0];
                auto &uv2 = glyph->UV[1];
                auto &uv3 = glyph->UV[2];
                auto &uv4 = glyph->UV[3];

                m_SubmitInfo.vertices = {
                    { { _x1, _y1 }, uv1, col },
                    { { _x1, _y2 }, uv4, col },
                    { { _x2, _y2 }, uv3, col },

                    { { _x1, _y1 }, uv1, col },
                    { { _x2, _y2 }, uv3, col },
                    { { _x2, _y1 }, uv2, col },
                };

                InsertToBatch();
            }

            pos += glyph->Advance * scale;
        }

        posy += m_FontAtlas->NewlineHeight * scale;
    }
}

glm::vec2 Text::MeasureString(std::string text)
{
    float width = 0.0f;
    float height = 0.0f;

    auto  strings = split(text, '\n');
    float scale = (m_FontAtlas->FontSize * Scale) / m_FontAtlas->FontSize;

    for (auto &string : strings) {
        float lineWidth = 0.0f;

        auto it = string.c_str();
        auto end = string.c_str() + strlen(it);

        while (it < end) {
            uint32_t character = *it;

            if (character >= 0x80) {
                it += charFromUTF8(&character, it, end);
            } else {
                it++;
            }

            if (character == '\r')
                continue;

            Fonts::Glyph *glyph = FindGlyph(character);
            lineWidth += glyph->Advance * scale;
        }

        width = std::max(width, lineWidth);
        height += m_FontAtlas->NewlineHeight * scale;
    }

    return glm::vec2(width, height);
}

Fonts::Glyph *Text::FindGlyph(uint32_t c)
{
    auto glyph_it = std::find_if(m_FontAtlas->Glyphs.begin(), m_FontAtlas->Glyphs.end(), [c](const Fonts::Glyph &glyph) -> bool {
        return glyph.Character == c;
    });

    Fonts::Glyph *glyph = nullptr;
    if (glyph_it == m_FontAtlas->Glyphs.end()) {
        glyph = &m_FontAtlas->Invalid;
    } else {
        glyph = &(*glyph_it);
    }

    return glyph;
}