#include "Texture/Texture2D.h"
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <memory>
#include <stdexcept>

#include "../Data/Imgui/imgui_impl_vulkan.h"
#include "../Rendering/Vulkan/Texture2DVulkan_Internal.h"
#include "Exception/SDLException.h"
#include "Rendering/Renderer.h"
#include "Rendering/Vulkan/Texture2DVulkan.h"
#include "Rendering/Vulkan/VulkanEngine.h"
#include "Texture/MathUtils.h"
#include <SDL2/SDL_image.h>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/glm.hpp>

namespace {
    // Premultiply alpha
    SDL_Surface* PremultiplyAlpha(SDL_Surface* surface) {
        if (surface->format->BytesPerPixel != 4) return surface;

        Uint32* pixels = (Uint32*)surface->pixels;
        for (int y = 0; y < surface->h; ++y) {
            for (int x = 0; x < surface->w; ++x) {
                Uint32 pixel = pixels[y * surface->w + x];
                Uint8 r, g, b, a;
                SDL_GetRGBA(pixel, surface->format, &r, &g, &b, &a);

                r = (r * a) / 255;
                g = (g * a) / 255;
                b = (b * a) / 255;

                pixels[y * surface->w + x] = SDL_MapRGBA(surface->format, r, g, b, a);
            }
        }
        return surface;
    }
}

Texture2D::Texture2D()
{
    TintColor = { 1.0f, 1.0f, 1.0f };

    Rotation = 0;
    Transparency = 0.0f;
    AlphaBlend = false;

    m_actualSize = {};
    m_preAnchoredSize = {};
    m_calculatedSize = {};

    m_sdl_surface = nullptr;
    m_sdl_tex = nullptr;

    m_bDisposeTexture = false;

    Size = UDim2::fromScale(1, 1);
}

Texture2D::Texture2D(const std::string& fileName) : Texture2D()
{
    std::string filePath = fileName;
    if (!std::filesystem::exists(filePath)) {
        filePath = std::filesystem::current_path().string() + fileName;
    }

    if (!std::filesystem::exists(filePath)) {
        throw std::runtime_error(fileName + " not found!");
    }

    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error(fileName + " cannot be opened!");
    }

    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    file.close();

    LoadImageResources(buffer.data(), size);
}

Texture2D::Texture2D(const std::filesystem::path& path) : Texture2D() {
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error(path.string() + " not found!");
    }

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error(path.string() + " cannot be opened!");
    }

    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    file.close();

    LoadImageResources(buffer.data(), size);
}

Texture2D::Texture2D(const uint8_t* fileData, size_t size) : Texture2D()
{
    std::vector<uint8_t> buffer(fileData, fileData + size);
    LoadImageResources(buffer.data(), size);
}

Texture2D::Texture2D(SDL_Texture *texture) : Texture2D()
{
    m_bDisposeTexture = false;
    m_sdl_tex = texture;
    m_ready = true;
}

Texture2D::Texture2D(Texture2D_Vulkan *texture) : Texture2D()
{
    m_bDisposeTexture = false;
    m_vk_tex = texture;

    m_ready = true;
}

Texture2D::~Texture2D()
{
    if (m_bDisposeTexture) {
        if (Renderer::GetInstance()->IsVulkan() && m_vk_tex) {
            vkTexture::ReleaseTexture(m_vk_tex);
            m_vk_tex = nullptr;
        } else {
            if (m_sdl_tex) {
                SDL_DestroyTexture(m_sdl_tex);
                m_sdl_tex = nullptr;
            }
            if (m_sdl_surface) {
                SDL_FreeSurface(m_sdl_surface);
                m_sdl_surface = nullptr;
            }
        }
    }
}

void Texture2D::Draw()
{
    Draw(true);
}

void Texture2D::Draw(bool manualDraw)
{
    Draw(nullptr, manualDraw);
}

void Texture2D::Draw(Rect *clipRect)
{
    Draw(clipRect, true);
}

void Texture2D::Draw(Rect* clipRect, bool manualDraw)
{
    Renderer* renderer = Renderer::GetInstance();
    auto window = GameWindow::GetInstance();
    bool scaleOutput = window->IsScaleOutput();
    CalculateSize();

    if (!m_ready)
        return;

    if (renderer->IsVulkan() && m_vk_tex) {
        auto vulkan_driver = renderer->GetVulkanEngine();
        auto cmd = vulkan_driver->get_current_frame()._mainCommandBuffer;

        VkRect2D scissor = {};
        scissor.offset.x = 0;
        scissor.offset.y = 0;
        scissor.extent.width = window->GetWidth();
        scissor.extent.height = window->GetHeight();

        if (clipRect) {
            scissor.offset.x = clipRect->left;
            scissor.offset.y = clipRect->top;
            scissor.extent.width = clipRect->right - clipRect->left;
            scissor.extent.height = clipRect->bottom - clipRect->top;
        }

        VkDescriptorSet imageId = m_vk_tex->DS;

        float x1 = m_calculatedSizeF.left;
        float y1 = m_calculatedSizeF.top;
        float x2 = m_calculatedSizeF.left + m_calculatedSizeF.right;
        float y2 = m_calculatedSizeF.top + m_calculatedSizeF.bottom;

        if (scaleOutput) {
            if (clipRect) {
                scissor.offset.x = static_cast<int32_t>(clipRect->left * window->GetWidthScale());
                scissor.offset.y = static_cast<int32_t>(clipRect->top * window->GetHeightScale());
                scissor.extent.width = static_cast<int32_t>((clipRect->right - clipRect->left) * window->GetWidthScale());
                scissor.extent.height = static_cast<int32_t>((clipRect->bottom - clipRect->top) * window->GetHeightScale());
            }

            x1 *= window->GetWidthScale();
            y1 *= window->GetHeightScale();
            x2 *= window->GetWidthScale();
            y2 *= window->GetHeightScale();
        }

        if (x2 <= 0 || y2 <= 0) {
            return;
        }

        ImVec2 uv1(0.0f, 0.0f); // Top-left UV coordinate
        ImVec2 uv2(1.0f, 0.0f); // Top-right UV coordinate
        ImVec2 uv3(1.0f, 1.0f); // Bottom-right UV coordinate
        ImVec2 uv4(0.0f, 1.0f); // Bottom-left UV coordinate

        ImU32 color = IM_COL32((uint8_t)(TintColor.R * 255), (uint8_t)(TintColor.G * 255), (uint8_t)(TintColor.B * 255), 255);

        std::array<ImDrawVert, 6> vertexData = {{
            {ImVec2(x1, y1), uv1, color},
            {ImVec2(x2, y1), uv2, color},
            {ImVec2(x2, y2), uv3, color},
            {ImVec2(x1, y1), uv1, color},
            {ImVec2(x2, y2), uv3, color},
            {ImVec2(x1, y2), uv4, color}
        }};

        SubmitQueueInfo info = {};
        info.AlphaBlend = AlphaBlend;
        info.descriptor = imageId;
        info.vertices = {vertexData.begin(), vertexData.end()};
        info.indices = {0, 1, 2, 3, 4, 5};
        info.scissor = scissor;

        vulkan_driver->queue_submit(info);
    } else {
        SDL_FRect destRect = {m_calculatedSizeF.left, m_calculatedSizeF.top, m_calculatedSizeF.right, m_calculatedSizeF.bottom};
        if (scaleOutput) {
            destRect.x *= window->GetWidthScale();
            destRect.y *= window->GetHeightScale();
            destRect.w *= window->GetWidthScale();
            destRect.h *= window->GetHeightScale();
        }

        SDL_Rect originClip = {};
        SDL_BlendMode oldBlendMode = SDL_BLENDMODE_NONE;

        if (clipRect) {
            SDL_RenderGetClipRect(renderer->GetSDLRenderer(), &originClip);

            SDL_Rect testClip = {clipRect->left, clipRect->top, clipRect->right - clipRect->left, clipRect->bottom - clipRect->top};
            if (scaleOutput) {
                testClip.x = static_cast<int>(testClip.x * window->GetWidthScale());
                testClip.y = static_cast<int>(testClip.y * window->GetHeightScale());
                testClip.w = static_cast<int>(testClip.w * window->GetWidthScale());
                testClip.h = static_cast<int>(testClip.h * window->GetHeightScale());
            }

            SDL_RenderSetClipRect(renderer->GetSDLRenderer(), &testClip);
        }

        if (AlphaBlend) {
            SDL_GetTextureBlendMode(m_sdl_tex, &oldBlendMode);
            SDL_SetTextureBlendMode(m_sdl_tex, renderer->GetSDLBlendMode());
        }

        SDL_Color color = {(uint8_t)(TintColor.R * 255.0f), (uint8_t)(TintColor.G * 255.0f), (uint8_t)(TintColor.B * 255.0f), 255};
        if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
            std::swap(color.r, color.b);
        }

        SDL_SetTextureColorMod(m_sdl_tex, color.r, color.g, color.b);
        SDL_SetTextureAlphaMod(m_sdl_tex, static_cast<uint8_t>(255 - (Transparency / 100.0) * 255));

        int error = SDL_RenderCopyExF(renderer->GetSDLRenderer(), m_sdl_tex, nullptr, &destRect, Rotation, nullptr, SDL_FLIP_NONE);

        if (error != 0) {
            throw SDLException();
        }

        if (AlphaBlend) {
            SDL_SetTextureBlendMode(m_sdl_tex, oldBlendMode);
        }

        if (clipRect) {
            SDL_RenderSetClipRect(renderer->GetSDLRenderer(), originClip.w == 0 || originClip.h == 0 ? nullptr : &originClip);
        }
    }
}

void Texture2D::CalculateSize()
{
    GameWindow* window = GameWindow::GetInstance();
    int wWidth = window->GetBufferWidth();
    int wHeight = window->GetBufferHeight();

    float xPos = static_cast<float>((wWidth * Position.X.Scale) + Position.X.Offset);
    float yPos = static_cast<float>((wHeight * Position.Y.Scale) + Position.Y.Offset);

    float width = static_cast<float>((m_actualSize.right * Size.X.Scale) + Size.X.Offset);
    float height = static_cast<float>((m_actualSize.bottom * Size.Y.Scale) + Size.Y.Offset);

    m_preAnchoredSize = {(LONG)xPos, (LONG)yPos, (LONG)width, (LONG)height};
    m_preAnchoredSizeF = {xPos, yPos, width, height};

    float xAnchor = width * std::clamp((float)AnchorPoint.X, 0.0f, 1.0f);
    float yAnchor = height * std::clamp((float)AnchorPoint.Y, 0.0f, 1.0f);

    xPos -= xAnchor;
    yPos -= yAnchor;

    m_calculatedSize = {(LONG)xPos, (LONG)yPos, (LONG)width, (LONG)height};
    m_calculatedSizeF = {xPos, yPos, width, height};

    AbsolutePosition = {xPos, yPos};
    AbsoluteSize = {width, height};
}

Rect Texture2D::GetOriginalRECT()
{
    return m_actualSize;
}

void Texture2D::SetOriginalRECT(Rect size)
{
    m_actualSize = size;
}

void Texture2D::LoadImageResources(uint8_t* buffer, size_t size)
{
    if (Renderer::GetInstance()->IsVulkan()) {
        auto tex_data = vkTexture::TexLoadImage(buffer, size);
        m_actualSize = {0, 0, tex_data->Width, tex_data->Height};
        m_vk_tex = tex_data;
        m_bDisposeTexture = true;
        m_ready = true;
    } else {
        SDL_RWops* rw = SDL_RWFromMem(buffer, static_cast<int>(size));
        std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> decompressed_surface(IMG_LoadTyped_RW(rw, 1, "PNG"), SDL_FreeSurface);

        if (!decompressed_surface) {
            throw SDLException();
        }

        std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> formatted_surface(SDL_ConvertSurfaceFormat(decompressed_surface.get(), SDL_PIXELFORMAT_ABGR8888, 0), SDL_FreeSurface);

        if (!formatted_surface) {
            throw SDLException();
        }

        m_sdl_surface = PremultiplyAlpha(formatted_surface.release()); // Fix white line issue
        m_sdl_tex = SDL_CreateTextureFromSurface(Renderer::GetInstance()->GetSDLRenderer(), m_sdl_surface);

        if (!m_sdl_tex) {
            throw SDLException();
        }

        m_actualSize = {0, 0, m_sdl_surface->w, m_sdl_surface->h};
        m_bDisposeTexture = true;
        m_ready = true;
    }
}