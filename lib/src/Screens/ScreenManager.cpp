/*
 * MIT License
 *
 * Copyright (c) 2023 Estrol Mendex
 * See the LICENSE file in the root of this project for details.
 */

#include <Exceptions/EstException.h>
#include <Game.h>
#include <Inputs/InputManager.h>
#include <Screens/ScreenManager.h>
using namespace Screens;

static Manager *instance = nullptr;

void Manager::Init(Game *game)
{
    m_Game = game;
    m_FadeRect = nullptr;
}

void Manager::Update(double delta)
{
    if (m_NextScreen != nullptr) {
        if (m_CurrentScreen != nullptr) {
            m_CurrentScreen->Detach();
        }
        m_CurrentScreen = m_NextScreen;
        m_CurrentScreen->Attach();
        m_NextScreen = nullptr;
    } else {
        if (m_CurrentScreen != nullptr) {
            m_CurrentScreen->Update(delta);
        }
    }

    while (!m_UpdateQueue.empty()) {
        m_UpdateQueue.front()();
        m_UpdateQueue.erase(m_UpdateQueue.begin());
    }
}

bool floatCompare(float a, float b)
{
    return fabs(a - b) < 0.0001;
}

void Manager::Draw(double delta)
{
    std::call_once(m_FadeTexFlag, [this] {
        m_FadeRect = std::make_shared<UI::Rectangle>();
    });

    if (m_CurrentScreen != nullptr) {
        m_CurrentScreen->Draw(delta);
    }

    while (!m_DrawQueue.empty()) {
        m_DrawQueue.front()();
        m_DrawQueue.erase(m_DrawQueue.begin());
    }

    if (m_FadeRect != nullptr) {
        if (floatCompare(m_FadeAlpha, m_TargetFadeAlpha)) {
            float increment = (static_cast<float>(delta) * 5.0f) * 100.0f;

            if (std::abs(m_FadeAlpha - m_TargetFadeAlpha) < FLT_EPSILON) {
                m_FadeAlpha = m_TargetFadeAlpha;
            } else {
                if (m_FadeAlpha < m_TargetFadeAlpha) {
                    m_FadeAlpha += increment;
                } else {
                    m_FadeAlpha -= increment;
                }
            }
        }

        auto window = Graphics::NativeWindow::Get();
        auto size = window->GetBufferSize();

        m_FadeRect->Size = UDim2::fromOffset(size.Width, size.Height);
        m_FadeRect->Transparency = m_FadeAlpha;
        m_FadeRect->Draw();
    }
}

void Manager::Input(double delta)
{
    if (m_CurrentScreen != nullptr) {
        m_CurrentScreen->Input(delta);
    }

    while (!m_InputQueue.empty()) {
        m_InputQueue.front()();
        m_InputQueue.erase(m_InputQueue.begin());
    }
}

void Manager::FixedUpdate(double fixedDelta)
{
    if (m_CurrentScreen != nullptr) {
        m_CurrentScreen->FixedUpdate(fixedDelta);
    }

    while (!m_FixedUpdateQueue.empty()) {
        m_FixedUpdateQueue.front()();
        m_FixedUpdateQueue.erase(m_FixedUpdateQueue.begin());
    }
}

void Manager::OnKeyDown(const Inputs::State &state)
{
    if (m_CurrentScreen != nullptr) {
        m_CurrentScreen->OnKeyDown(state);
    }
}

void Manager::OnKeyUp(const Inputs::State &state)
{
    if (m_CurrentScreen != nullptr) {
        m_CurrentScreen->OnKeyUp(state);
    }
}

void Manager::AddScreen(uint32_t Id, Base *screen)
{
    m_Screens[Id] = std::unique_ptr<Base>(screen);
}

void Manager::SetScreen(uint32_t Id)
{
    if (m_Screens.find(Id) == m_Screens.end()) {
        throw Exceptions::EstException("Screen not found");
    }

    m_CurrentScreen = m_Screens[Id].get();
    m_CurrentScreen->Attach();

    m_CurrentScreenId = Id;
}

uint32_t Manager::GetCurrentScreenId()
{
    return m_CurrentScreenId;
}

void Manager::Enqueue(EnqueueType type, std::function<void()> func)
{
    switch (type) {
        case EnqueueType::Update:
            m_UpdateQueue.push_back(func);
            break;

        case EnqueueType::Draw:
            m_DrawQueue.push_back(func);
            break;

        case EnqueueType::Input:
            m_InputQueue.push_back(func);
            break;

        case EnqueueType::FixedUpdate:
            m_FixedUpdateQueue.push_back(func);
            break;
    }
}

void Manager::Internal_DisplayFade(int transparency, std::function<void()> callback)
{
    std::thread([=]() {
        std::lock_guard<std::mutex> lock(this->m_FadeLock);

        m_TargetFadeAlpha = static_cast<float>(transparency);
        while (!floatCompare(m_FadeAlpha, m_TargetFadeAlpha)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        if (callback != nullptr) {
            callback();
        }
    }).detach();
}

void Manager::Internal_SetFadeColor(const Color3 &color)
{
    m_FadeRect->Color3 = color;
}

void Manager::DisplayFade(int transparency, std::function<void()> callback)
{
    Get()->Internal_DisplayFade(transparency, callback);
}

void Manager::SetFadeColor(const Color3 &color)
{
    Get()->Internal_SetFadeColor(color);
}

Manager *Manager::Get()
{
    if (instance == nullptr) {
        instance = new Manager();
    }
    return instance;
}

void Manager::Destroy()
{
    if (instance != nullptr) {
        delete instance;
        instance = nullptr;
    }
}