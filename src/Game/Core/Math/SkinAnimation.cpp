#include "SkinAnimation.h"
#include <Exceptions/EstException.h>

SkinAnimation::SkinAnimation(AnimationInfo &info) : m_Info(info)
{
}

SkinAnimation::~SkinAnimation()
{
}

UDim2 SkinAnimation::Update(double delta)
{
    m_CurrentTime += delta;

    if (m_CurrentTime > m_Info.Duration) {
        if (m_Info.Repeat) {
            m_CurrentTime = 0.0;
        } else {
            m_CurrentTime = m_Info.Duration;
        }
    }

    auto alpha = m_CurrentTime / m_Info.Duration;
    return Calculate(alpha);
}

UDim2 SkinAnimation::Calculate(double alpha)
{
    if (!m_Info.Callback.valid()) {
        throw Exceptions::EstException("Callback is not valid");
    }

    sol::safe_function_result result = m_Info.Callback(
        m_Info.Position.Start,
        m_Info.Position.End,
        alpha);

    if (!result.valid()) {
        sol::error err = result;
        throw Exceptions::EstException("Error in callback: %s", err.what());
    }

    return result.get<UDim2>();
}