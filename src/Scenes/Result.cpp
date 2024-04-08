#include "Result.h"

#include "../Game/Core/Skinning/LuaSkin.h"
#include "../Game/Env.h"

#include <MsgBox.h>

Result::Result()
{
    m_IsAttached = false;
}

Result::~Result()
{
}

void Result::Update(double delta)
{
    (void)delta;
}

void Result::Draw(double delta)
{
    if (!m_IsAttached) {
        return;
    }

    m_Result->Draw();

    m_Score->Draw(m_ScoreInfo.score);

    if (m_ScoreInfo.isClear) {
        m_WinLose->SetIndexAt(0);
    } else {
        m_WinLose->SetIndexAt(1);
    }

    m_WinLose->Draw();
    m_BackButton->Draw();

    m_Stats->Position = m_StatsPosition[0];
    m_Stats->AnchorPoint = m_StatsAnchor[0];
    m_Stats->Draw(m_ScoreInfo.cool);

    m_Stats->Position = m_StatsPosition[1];
    m_Stats->AnchorPoint = m_StatsAnchor[1];
    m_Stats->Draw(m_ScoreInfo.good);

    m_Stats->Position = m_StatsPosition[2];
    m_Stats->AnchorPoint = m_StatsAnchor[2];
    m_Stats->Draw(m_ScoreInfo.bad);

    m_Stats->Position = m_StatsPosition[3];
    m_Stats->AnchorPoint = m_StatsAnchor[3];
    m_Stats->Draw(m_ScoreInfo.miss);

    m_Stats->Position = m_StatsPosition[4];
    m_Stats->AnchorPoint = m_StatsAnchor[4];
    m_Stats->Draw(m_ScoreInfo.maxCombo);

    m_Stats->Position = m_StatsPosition[5];
    m_Stats->AnchorPoint = m_StatsAnchor[5];
    m_Stats->Draw(m_ScoreInfo.maxJam);

    (void)delta;
}

void Result::Input(double delta)
{
    if (!m_IsAttached) {
        return;
    }

    if (m_BackButton->UpdateInput()) {
        return;
    }

    (void)delta;
}

bool Result::Attach()
{
    auto manager = LuaManager::Get();
    m_ResultLua = manager->LoadScript(SkinGroup::Result);

    auto resultPos = m_ResultLua->GetPosition("Result").front();
    m_Result = std::make_shared<Image>(resultPos.Path);
    m_Result->Position = resultPos.Position;
    m_Result->Size = resultPos.Size;
    m_Result->AnchorPoint = resultPos.AnchorPoint;
    m_Result->Color3 = resultPos.Color;

    auto scorePos = m_ResultLua->GetPosition("Score").front();
    auto scoreImgs = m_ResultLua->GetNumeric("Score").front();
    m_Score = std::make_shared<NumberSprite>(scoreImgs.Path, scoreImgs.TexCoords);
    m_Score->Size = scoreImgs.Size;
    m_Score->Position = scorePos.Position;
    m_Score->AnchorPoint = scorePos.AnchorPoint;
    m_Score->NumberPosition = IntToPos(scoreImgs.Direction);
    m_Score->FillWithZeros = scoreImgs.FillWithZero;

    auto winLose = m_ResultLua->GetSprite("WinLose");
    m_WinLose = std::make_shared<Sprite>(winLose.Path, winLose.TexCoords, 0.0f);
    m_WinLose->Position = winLose.Position;
    m_WinLose->Size = winLose.Size;
    m_WinLose->AnchorPoint = winLose.AnchorPoint;
    m_WinLose->Color3 = winLose.Color;

    auto backRect = m_ResultLua->GetRect("Back").front();
    auto backHoverButton = m_ResultLua->GetSprite("BackHoverButton");

    std::shared_ptr<Sprite> backButtonSprite = std::make_shared<Sprite>(backHoverButton.Path, backHoverButton.TexCoords, 0.0f);
    backButtonSprite->Position = backHoverButton.Position;
    backButtonSprite->Size = backHoverButton.Size;
    backButtonSprite->AnchorPoint = backHoverButton.AnchorPoint;
    backButtonSprite->Color3 = backHoverButton.Color;

    m_BackButton = std::make_shared<ButtonImage>(
        Rect{
            (int)backRect.Position.X.Offset,
            (int)backRect.Position.Y.Offset,
            (int)backRect.Size.X.Offset,
            (int)backRect.Size.Y.Offset },
        backButtonSprite);

    m_BackButton->OnClick([]() { MsgBox::Show("Hello", "Hello"); });

    auto coolPos = m_ResultLua->GetPosition("StatsCool").front();
    auto goodPos = m_ResultLua->GetPosition("StatsGood").front();
    auto badPos = m_ResultLua->GetPosition("StatsBad").front();
    auto missPos = m_ResultLua->GetPosition("StatsMiss").front();
    auto maxcomboPos = m_ResultLua->GetPosition("StatsMaxCombo").front();
    auto maxjamPos = m_ResultLua->GetPosition("StatsMaxJam").front();

    m_StatsPosition = {
        { 0, coolPos.Position },
        { 1, goodPos.Position },
        { 2, badPos.Position },
        { 3, missPos.Position },
        { 4, maxcomboPos.Position },
        { 5, maxjamPos.Position }
    };

    m_StatsAnchor = {
        { 0, coolPos.AnchorPoint },
        { 1, goodPos.AnchorPoint },
        { 2, badPos.AnchorPoint },
        { 3, missPos.AnchorPoint },
        { 4, maxcomboPos.AnchorPoint },
        { 5, maxjamPos.AnchorPoint }
    };

    auto statsImgs = m_ResultLua->GetNumeric("Stats").front();
    m_Stats = std::make_shared<NumberSprite>(statsImgs.Path, statsImgs.TexCoords);
    m_Stats->NumberPosition = IntToPos(statsImgs.Direction);
    m_Stats->FillWithZeros = statsImgs.FillWithZero;
    m_Stats->Color3 = statsImgs.Color;
    m_Stats->Size = statsImgs.Size;

    m_ScoreInfo = ScoreInfo{
        .score = Env::GetInt("Score"),
        .cool = Env::GetInt("Cool"),
        .good = Env::GetInt("Good"),
        .bad = Env::GetInt("Bad"),
        .miss = Env::GetInt("Miss"),
        .maxJam = Env::GetInt("MaxJam"),
        .maxCombo = Env::GetInt("MaxCombo"),
        .isClear = Env::GetBool("IsClear")
    };

    m_IsAttached = true;
    return true;
}

bool Result::Detach()
{
    m_Result.reset();
    m_Text.reset();

    m_ResultLua.reset();
    return true;
}