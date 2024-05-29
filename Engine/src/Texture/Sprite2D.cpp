#include "Texture/Sprite2D.h"
#include "Rendering/Window.h"
#include "Texture/Texture2D.h"

Sprite2D::Sprite2D(std::vector<Texture2D *> textures, double delay) : Sprite2D::Sprite2D()
{
    m_textures = textures;
    m_spritespeed = delay;
    m_currentTime = 0.0;
    m_currentIndex = 0;

    Size = UDim2::fromScale(1, 1);
    Position = UDim2::fromOffset(0, 0);
    AnchorPoint = { 0, 0 };
}

Sprite2D::Sprite2D(std::vector<std::string> textures, double delay) : Sprite2D::Sprite2D()
{
    m_spritespeed = delay;
    Size = UDim2::fromScale(1, 1);
    Position = UDim2::fromOffset(0, 0);
    AnchorPoint = { 0, 0 };

    for (auto &it : textures) {
        m_textures.emplace_back(new Texture2D(it));
    }
}

Sprite2D::Sprite2D(std::vector<std::filesystem::path> textures, double delay) : Sprite2D::Sprite2D()
{
    m_spritespeed = delay;
    Size = UDim2::fromScale(1, 1);
    Position = UDim2::fromOffset(0, 0);
    AnchorPoint = { 0, 0 };

    assert(textures.size() > 0);

    for (auto &it : textures) {
        m_textures.emplace_back(new Texture2D(it));
    }
}

Sprite2D::Sprite2D(std::vector<SDL_Texture *> textures, double delay) : Sprite2D::Sprite2D()
{
    m_spritespeed = delay;
    Size = UDim2::fromScale(1, 1);
    Position = UDim2::fromOffset(0, 0);
    AnchorPoint = { 0, 0 };

    for (auto &it : textures) {
        m_textures.emplace_back(new Texture2D(it));
    }
}

Sprite2D::~Sprite2D()
{
    for (auto &it : m_textures) {
        delete it;
    }
}

void Sprite2D::Draw(double delta, bool manual)
{
    Draw(delta, nullptr, manual);
}

void Sprite2D::DrawInternal(double delta, bool playOnce, Rect* rect, bool manual)
{
    if (m_textures.empty()) return; // Safety check to ensure m_textures is not empty

    if (m_currentIndex >= m_textures.size()) {
        if (playOnce) {
            return; // Stop if playing once and reached end
        }
        m_currentIndex = 0; // Reset index if looping
    }

    auto tex = m_textures[m_currentIndex];
    GameWindow* window = GameWindow::GetInstance();

    double xPos = (window->GetBufferWidth() * Position.X.Scale) + (Position.X.Offset);
    double yPos = (window->GetBufferHeight() * Position.Y.Scale) + (Position.Y.Offset);

    double xMPos = (window->GetBufferWidth() * Position2.X.Scale) + (Position2.X.Offset);
    double yMPos = (window->GetBufferHeight() * Position2.Y.Scale) + (Position2.Y.Offset);

    xPos += xMPos;
    yPos += yMPos;

    tex->Position = UDim2::fromOffset(xPos, yPos);
    tex->AlphaBlend = AlphaBlend;
    tex->Size = Size;
    tex->AnchorPoint = AnchorPoint;
    tex->Draw(rect, manual ? false : true);

    if (m_spritespeed > 0.0) {
        m_currentTime += static_cast<float>(delta);
        if (m_currentTime >= m_spritespeed) {
            m_currentTime = 0.0;
            m_currentIndex++;

            if (m_currentIndex >= m_textures.size()) {
                if (playOnce) {
                    m_currentIndex = m_textures.size() - 1; // Stop at the last frame if playing once
                    return;
                } else {
                    m_currentIndex = 0; // Loop back to the first frame
                }
            }
        }
    }
}

void Sprite2D::Draw(double delta, Rect* rect, bool manual)
{
    DrawInternal(delta, false, rect, manual); // Play loop
}

void Sprite2D::DrawOnce(double delta, bool manual)
{
    DrawInternal(delta, true, nullptr, manual); // Play once
}

void Sprite2D::DrawStop(double delta, bool manual)
{
    DrawInternal(delta, true, nullptr, manual);

    if (m_currentIndex >= m_textures.size()) { // Play the stop on last frame
        m_currentIndex = m_textures.size() - 1;
    }
}

void Sprite2D::Reset()
{
    m_currentIndex = 0;
    m_currentTime = 0.0;
}

Texture2D *Sprite2D::GetTexture()
{
    if (m_textures.empty()) return nullptr; // Safety check to ensure m_textures is not empty

    auto tex = m_textures[m_currentIndex];
    tex->Position = Position;
    tex->Size = Size;
    tex->AnchorPoint = AnchorPoint;

    return tex;
}

void Sprite2D::SetFPS(double fps)
{
    m_spritespeed = 1.0f / fps;
}
