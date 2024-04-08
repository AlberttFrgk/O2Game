/*
 * MIT License
 *
 * Copyright (c) 2023 Estrol Mendex
 * See the LICENSE file in the root of this project for details.
 */

#ifndef __SCREENSMANAGER_H_
#define __SCREENSMANAGER_H_

#include "Base.h"
#include <functional>
#include <memory>
#include <unordered_map>
#include <Exceptions/EstException.h>
#include <vector>

#include <UI/Rectangle.h>

class Game;

namespace Screens {
    enum EnqueueType {
        Update,
        Draw,
        Input,
        FixedUpdate
    };

    class Manager
    {
    public:
        void Init(Game *game);

        void Update(double delta);
        void Draw(double delta);
        void Input(double delta);
        void FixedUpdate(double fixedDelta);

        void OnKeyDown(const Inputs::State &state);
        void OnKeyUp(const Inputs::State &state);

        void     AddScreen(uint32_t Id, Base *screen);
        void     SetScreen(uint32_t Id);
        uint32_t GetCurrentScreenId();

        template <class T>
        void Add()
        {
            static_assert(std::is_base_of<Base, T>::value, "T must be a subclass of Base");

            m_Screens.insert(std::make_pair(T::GetId(), std::make_unique<T>()));
        }

        template <class T>
        void Set()
        {
            static_assert(std::is_base_of<Base, T>::value, "T must be a subclass of Base");

            bool found = false;
            for (auto &screen : m_Screens) {
                if (dynamic_cast<T *>(screen.second.get())) {
                    m_NextScreen = screen.second.get();
                    found = true;
                    break;
                }
            }

            if (!found) {
                throw Exceptions::EstException("Screen %s not found", typeid(T).name());
            }
        }

        void Enqueue(EnqueueType type, std::function<void()> func);

        void Internal_DisplayFade(int transparency, std::function<void()> callback);
        void Internal_SetFadeColor(const Color3 &color);

        static void DisplayFade(int transparency, std::function<void()> callback);
        static void SetFadeColor(const Color3 &color);

        static Manager *Get();
        static void     Destroy();

    private:
        Manager() = default;
        ~Manager() = default;

        std::unordered_map<uint32_t, std::unique_ptr<Base>> m_Screens;
        std::vector<std::function<void()>>                  m_UpdateQueue;
        std::vector<std::function<void()>>                  m_DrawQueue;
        std::vector<std::function<void()>>                  m_InputQueue;
        std::vector<std::function<void()>>                  m_FixedUpdateQueue;

        Game    *m_Game;
        Base    *m_CurrentScreen;
        Base    *m_NextScreen;
        uint32_t m_CurrentScreenId = -1;

        float                          m_FadeAlpha = 0.0f;
        float                          m_TargetFadeAlpha = 0.0f;
        std::shared_ptr<UI::Rectangle> m_FadeRect;
        std::mutex                     m_FadeLock;

        std::once_flag m_FadeTexFlag;
    };
} // namespace Screens

#endif