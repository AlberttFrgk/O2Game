#include "DrawableNote.hpp"
#include "../Resources/GameResources.hpp"
#include "Rendering/Renderer.h"
#include "Texture/Texture2D.h"

DrawableNote::DrawableNote(NoteImage* frame) : FrameTimer::FrameTimer()
{
    m_frames.reserve(frame->Texture.size()); // Reserve memory for frames vector

    if (Renderer::GetInstance()->IsVulkan()) {
        for (auto& texture : frame->VulkanTexture) {
            m_frames.emplace_back(new Texture2D(texture));
            m_frames.back()->SetOriginalRECT(frame->TextureRect); // Set original RECT for each texture
        }
    }
    else {
        for (auto& texture : frame->Texture) {
            m_frames.emplace_back(new Texture2D(texture));
            m_frames.back()->SetOriginalRECT(frame->TextureRect); // Set original RECT for each texture
        }
    }

    AnchorPoint = { 0.0, 1.0 };

    SetFPS(0); // slighly fix stupid glitch
}
