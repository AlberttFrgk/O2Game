#include "FrameTimer.hpp"
#include "Rendering/Renderer.h"
#include "Texture/Texture2D.h"

FrameTimer::FrameTimer() :
    Repeat(false),
    m_currentFrame(0),
    m_frameTime(1.0 / 60.0),
    m_currentTime(0),
    AlphaBlend(false),
    Size(UDim2::fromScale(1, 1)),
    TintColor({1.0f, 1.0f, 1.0f})
{
}

FrameTimer::FrameTimer(std::vector<std::shared_ptr<Texture2D>> frames) : FrameTimer()
{
    m_frames = std::move(frames);
}

FrameTimer::FrameTimer(std::vector<std::string> frames) : FrameTimer()
{
    for (const auto &frame : frames) {
        m_frames.emplace_back(std::make_shared<Texture2D>(frame));
    }
}

FrameTimer::FrameTimer(std::vector<std::filesystem::path> frames) : FrameTimer()
{
    for (const auto &frame : frames) {
        m_frames.emplace_back(std::make_shared<Texture2D>(frame));
    }
}

FrameTimer::FrameTimer(std::vector<SDL_Texture *> frames) : FrameTimer()
{
    for (const auto &frame : frames) {
        m_frames.emplace_back(std::make_shared<Texture2D>(frame));
    }
}

FrameTimer::FrameTimer(std::vector<Texture2D_Vulkan *> frames) : FrameTimer()
{
    for (const auto &frame : frames) {
        m_frames.emplace_back(std::make_shared<Texture2D>(frame));
    }
}

FrameTimer::~FrameTimer()
{
    // The destructor is empty because using shared_ptr
}

void FrameTimer::Draw(double delta)
{
    Draw(delta, nullptr);
}

void FrameTimer::Draw(double delta, Rect *clip)
{
    m_currentTime += delta;

    if (m_currentTime >= m_frameTime) {
        m_currentTime -= m_frameTime;
        m_currentFrame++;
    }

    if (Repeat && m_currentFrame >= m_frames.size()) {
        m_currentFrame = 0;
    }

    if (m_currentFrame < m_frames.size()) {
        CalculateSize();

        auto currentFrame = m_frames[m_currentFrame];
        currentFrame->AlphaBlend = AlphaBlend;
        currentFrame->TintColor = TintColor;
        if (m_currentFrame != 0) {
            currentFrame->Position = UDim2::fromOffset(AbsolutePosition.X, AbsolutePosition.Y);
            currentFrame->Size = UDim2::fromOffset(AbsoluteSize.X, AbsoluteSize.Y);
            currentFrame->AnchorPoint = {0, 0};
        }

        currentFrame->Draw(clip);
    }
}

void FrameTimer::SetFPS(double fps)
{
    m_frameTime = 1.0 / fps;
}

void FrameTimer::ResetIndex()
{
    m_currentFrame = 0;
}

void FrameTimer::LastIndex()
{
    m_currentFrame = static_cast<int>(m_frames.size()) - 1;
}

void FrameTimer::SetIndexAt(int idx)
{
    m_currentFrame = idx;
}

void FrameTimer::CalculateSize()
{
    if (!m_frames.empty()) {
        auto &firstFrame = *m_frames[0];
        firstFrame.AnchorPoint = AnchorPoint;
        firstFrame.Size = Size;
        firstFrame.Position = Position;
        firstFrame.CalculateSize();

        AbsoluteSize = firstFrame.AbsoluteSize;
        AbsolutePosition = firstFrame.AbsolutePosition;
    }
}
