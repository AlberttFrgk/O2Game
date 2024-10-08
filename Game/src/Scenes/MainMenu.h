#pragma once
#include "../Engine/SkinConfig.hpp"
#include "Scene.h"
#include "Texture/Texture2D.h"

class MainMenu : public Scene
{
public:
    MainMenu();

    void Update(double delta) override;
    void Render(double delta) override;

    bool Attach() override;
    bool Detach() override;

private:
    std::unique_ptr<Texture2D> m_background;
    int                        m_targetScene = -1;

    SkinConfig m_skin;
};