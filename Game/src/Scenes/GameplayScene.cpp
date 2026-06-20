#include "GameplayScene.h"
#include <Exception/SDLException.h>

#include <Logs.h>
#include <future>
#include <iostream>
#include <limits.h>
#include <numeric>
#include <random>
#include <unordered_map>

#include "Configuration.h"
#include "Game.h"
#include "MsgBox.h"
#include "Rendering/Window.h"
#include "SceneManager.h"

#include "Imgui/ImguiUtil.h"
#include "Imgui/imgui.h"
#include "Texture/ImageGenerator.h"
#include "Texture/MathUtils.h"

#include "../Engine/NoteImageCacheManager.hpp"
#include "../Engine/SkinConfig.hpp"
#include "../Engine/SkinManager.hpp"
#include "../Engine/VideoPlayer.hpp"
#include "../EnvironmentSetup.hpp"
#include "../Data/Util/Util.hpp"
#include "../GameScenes.h"

#define AUTOPLAY_TEXT u8"Game currently on autoplay!"
#define GAMEINFO_TEXT u8"O2Clone Build Date: " __DATE__ " " __TIME__

struct MissInfo {
  int type;
  float beat;
  float hit_beat;
  float time;
};

bool CheckSkinComponent(std::filesystem::path x) {
  return std::filesystem::exists(x);
}

GameplayScene::GameplayScene() : Scene::Scene() {
  m_keyLighting = {};
  m_keyButtons = {};
  m_keyState = {};
  m_game = nullptr;
  m_drawJam = false;
  m_counter = 0.0;
  lifeFillDuration = 0.0;
}

GameplayScene::~GameplayScene() = default;

void GameplayScene::Update(double delta) {
  if (m_resourceFucked) {
    if (!m_ended) {
      if (EnvironmentSetup::GetInt("Key") >= 0) {
        m_ended = true;
        SceneManager::ChangeScene(GameScene::SONGSELECT);
      } else {
        if (MsgBox::GetResult("GameplayError") == 4) {
          m_ended = true;
          SceneManager::GetInstance()->StopGame();
        }
      }
    }

    return;
  }

  if (!m_starting) {
    EnvironmentSetup::SetInt("FillStart", 1);
    m_starting = true;
    lifeFillDuration = 0.0;
  }

  if (m_starting && m_game->GetState() == GameState::NotGame) {
    lifeFillDuration += delta;
    if (lifeFillDuration > 1.50 && EnvironmentSetup::GetInt("FillStart") == 1) {
      EnvironmentSetup::SetInt("FillStart", 0);
    }
    if (lifeFillDuration > 2.50) {
      m_game->Start();
    }
  }

  if (m_game->GetState() == GameState::PosGame && !m_ended) {
    m_counter += delta;
    m_drawExitButton = false;
    m_doExit = false;
    if (m_counter > 2.0) {
      m_ended = true;
      m_counter = 0.0; // Reset
      SceneManager::DisplayFade(
          100, [] { SceneManager::ChangeScene(GameScene::RESULT); });
    }
  }

  if (m_game->GetState() == GameState::Fail && !m_ended) {
    m_ended = true;
    m_counter = 0.0; // Reset
    SceneManager::DisplayFade(
        100, [] { SceneManager::ChangeScene(GameScene::RESULT); });
  }

  int difficulty = EnvironmentSetup::GetInt("Difficulty");
  float health = m_game->GetScoreManager()->GetLife();
  if (difficulty >= 1 && m_starting) {
    if (health <= 0) {
      m_game->Fail();
      EnvironmentSetup::SetInt("NowPlaying", 0);
      EnvironmentSetup::SetInt("Failed", 1);
    }
  } else {
    if (health <= 0) {
      EnvironmentSetup::SetInt("Failed", 1);
    }
  }

  if (m_doExit && !m_ended) {
    m_ended = true;

    auto scores = m_game->GetScoreManager()->GetScore();

    if (std::get<1>(scores) != 0 || std::get<2>(scores) != 0 ||
        std::get<3>(scores) != 0 || std::get<4>(scores) != 0) {
      if (m_game->GetState() == GameState::PosGame) {
        EnvironmentSetup::SetInt("Failed", 0);
      } else {
        EnvironmentSetup::SetInt("Failed", 1);
      }
      SceneManager::DisplayFade(
          100, [] { SceneManager::ChangeScene(GameScene::RESULT); });
    } else {
      SceneManager::DisplayFade(
          100, [] { SceneManager::ChangeScene(GameScene::SONGSELECT); });
    }
  }

  m_exitButtonFunc->Input(delta);
  m_game->Update(delta);
}

void GameplayScene::Render(double delta) {
  if (m_resourceFucked) {
    return;
  }

  ImguiUtil::NewFrame();

  int arena = EnvironmentSetup::GetInt("Arena");

  Chart *chart = (Chart *)EnvironmentSetup::GetObj("SONG");
  bool isGameActive = m_game && (m_game->GetState() == GameState::Playing || m_game->GetState() == GameState::PosGame);
  bool hasActiveVisuals = isGameActive && (m_videoPlayer && m_videoPlayer->IsValid());

  if (!hasActiveVisuals) {
      if (EnvironmentSetup::GetInt("Background") == 1) {
        auto songBG = (Texture2D *)EnvironmentSetup::GetObj("SongBackground");
        if (songBG) {
          int dim = EnvironmentSetup::GetInt("BackgroundDim");
          if (dim < 0) dim = 0;
          if (dim > 100) dim = 100;
          int colorVal = (int)(255.0f * (100.0f - dim) / 100.0f);
          songBG->TintColor = Color3::FromRGB(colorVal, colorVal, colorVal);
          songBG->Draw();
        }
      } else if (EnvironmentSetup::GetInt("Background") == 2) {
        // Do nothing
      } else {
        if (m_PlayBG) {
            m_PlayBG->Draw();
        }
      }
  }

  if (m_videoPlayer && m_videoPlayer->IsValid() && chart && isGameActive) {
      m_videoPlayer->Update(m_game->GetGameAudioPosition() - chart->m_videoOffset);
      m_videoPlayer->Render();
  }

  if (m_Playfield) m_Playfield->Draw();
  if (m_PlayFooter) m_PlayFooter->Draw();

  if (m_targetBar && m_game) {
    int maxFrames = m_targetBar->GetFrameCount();
    if (maxFrames > 0) {
      double bpm = m_game->GetCurrentBPM();
      if (bpm > 0.0) {
        m_targetBar->SetIndex(m_game->GetBPMAnimationIndex(maxFrames));
      }
    }
    m_targetBar->Draw(delta);
    m_targetBar->AlphaBlend = true;
  } else if (m_targetBar) {
    m_targetBar->Draw(delta);
    m_targetBar->AlphaBlend = true;
  }

  for (auto &[lane, pressed] : m_keyState) {
    if (pressed) {
      m_keyLighting[lane]->AlphaBlend = true;
      m_keyLighting[lane]->Draw(delta);
      m_keyDowns[lane]->Draw(delta);
    } else {
      m_keyButtons[lane]->Draw(delta);
    }
  }

  m_game->Render(delta);

  // Draw Mods
  if (m_noteMod) {
    m_noteMod->Draw();
  }
  if (m_visualMod) {
    m_visualMod->Draw();
    m_laneHideImage->Draw();
  }

  auto scores = m_game->GetScoreManager()->GetScore();
  m_scoreNum->DrawNumber(std::get<0>(scores));

  // Draw stats
  {
    m_statsNum->Position = m_statsPos[0];
    m_statsNum->DrawNumber(std::get<1>(scores));
    m_statsNum->Position = m_statsPos[1];
    m_statsNum->DrawNumber(std::get<2>(scores));
    m_statsNum->Position = m_statsPos[2];
    m_statsNum->DrawNumber(std::get<3>(scores));
    m_statsNum->Position = m_statsPos[3];
    m_statsNum->DrawNumber(std::get<4>(scores));
    m_statsNum->Position = m_statsPos[4];
    m_statsNum->DrawNumber(std::get<8>(scores));
  }

  int numOfPills = m_game->GetScoreManager()->GetPills();
  for (int i = 0; i < numOfPills; i++) {
    m_pills[i]->Draw();
  }

  if (m_lifeBar && m_game) {
    int maxFrames = m_lifeBar->GetFrameCount();
    if (maxFrames > 0) {
      double bpm = m_game->GetCurrentBPM();
      if (bpm > 0.0) {
        m_lifeBar->SetIndex(m_game->GetBPMAnimationIndex(maxFrames));
      }
    }
  }

  bool fillstart = EnvironmentSetup::GetInt("FillStart") == 1;

  if (fillstart) {
    float progress = 0.0f;
    if (lifeFillDuration > 0.500) {
      progress = static_cast<float>((lifeFillDuration - 0.500) / 1.0);
    }
    if (progress > 1.0f)
      progress = 1.0f;

    float fillRatio = progress;
    float alpha = 1.0f - fillRatio;

    auto curLifeTex = m_lifeBar->GetTexture();
    curLifeTex->CalculateSize();

    Rect rc = {};
    rc.left = static_cast<int>(curLifeTex->AbsolutePosition.X);
    rc.top = static_cast<int>(curLifeTex->AbsolutePosition.Y +
                              curLifeTex->AbsoluteSize.Y * (1.0f - fillRatio));
    rc.right = static_cast<int>(rc.left + curLifeTex->AbsoluteSize.X);
    rc.bottom = static_cast<int>(rc.top + curLifeTex->AbsoluteSize.Y);

    m_lifeBar->Draw(delta, &rc);
  } else {

    float alpha =
        (float)(kMaxLife - m_game->GetScoreManager()->GetLife()) / kMaxLife;

    auto curLifeTex = m_lifeBar->GetTexture();
    curLifeTex->CalculateSize();

    float offset =
        10.0f +
        (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 5.0f;

    Rect rc = {};
    rc.left = static_cast<int>(curLifeTex->AbsolutePosition.X);
    rc.top = static_cast<int>(curLifeTex->AbsolutePosition.Y);
    rc.right = static_cast<int>(rc.left + curLifeTex->AbsoluteSize.X);
    rc.bottom = static_cast<int>(rc.top + curLifeTex->AbsoluteSize.Y);

    double wiggle =
        sinf(static_cast<float>(m_game->GetGameFrame()) * 60.0f) * offset;

    int maxTop = static_cast<int>(curLifeTex->AbsolutePosition.Y);
    int topCur = (int)::round((1.0f - alpha) * maxTop + alpha * rc.bottom);

    rc.top = topCur + static_cast<int>(::round(wiggle));

    if (rc.top < maxTop) {
      rc.top = maxTop;
    }
    if (rc.top >= rc.bottom) {
      rc.top = rc.bottom - 1;
    }

    m_lifeBar->Draw(delta, &rc);
  }

  if (m_drawJudge) {
    if (m_judgementSprite.find(m_judgeIndex) != m_judgementSprite.end() && m_judgementSprite[m_judgeIndex] != nullptr) {
        m_judgementSprite[m_judgeIndex]->AnchorPoint = {0.5, 0.5};
        m_judgementSprite[m_judgeIndex]->DrawOnce(delta);

        if ((m_judgeTimer += delta) > 0.60) {
            m_drawJudge = false;
        }
    } else if (m_judgement[m_judgeIndex] != nullptr) {
        m_judgement[m_judgeIndex]->Size =
            UDim2::fromScale(m_judgeSize, m_judgeSize);
        m_judgement[m_judgeIndex]->AnchorPoint = {0.5, 0.5};
        m_judgement[m_judgeIndex]->Draw();

        m_judgeSize = std::clamp(m_judgeSize + (delta * 6), 0.4, 1.0); // Nice
        if ((m_judgeTimer += delta) > 0.60) {
          m_drawJudge = false;
        }
    }
  }

  if (m_drawJam) {
    if (std::get<5>(scores) > 0) {
      m_jamNum->DrawNumber(std::get<5>(scores));
      m_jamLogo->DrawStop(delta); // Example for DrawStop
    }

    if ((m_jamTimer += delta) > 0.60) {
      m_drawJam = false;
    }
  }

  if (m_drawCombo &&
      std::get<7>(scores) > 0) { // O2Jam Replication by Albert Frengki!
    const double positionStart = 30.0;

    // Animate over 0.10 seconds
    double t = std::clamp(m_comboTimer / 0.10, 0.0, 1.0);

    // Ease-Out Cubic curve for a fast, clean stop without bouncing
    double t_m_1 = t - 1.0;
    double easeOut = 1.0 + (t_m_1 * t_m_1 * t_m_1);

    double currentPosition = positionStart * (1.0 - easeOut);

    m_comboLogo->Position2 = UDim2::fromOffset(0, currentPosition / 3.0);
    m_comboNum->Position2 = UDim2::fromOffset(0, currentPosition);

    m_comboLogo->Draw(delta);
    m_comboNum->DrawNumber(std::get<7>(scores));

    m_comboTimer += delta;

    if (m_comboTimer >= 1.0) {
      m_comboTimer = 0.0;
      m_drawCombo = false;
    }
  }

  if (m_drawLN && std::get<9>(scores) > 0) {
    const double positionStart = 5.0;

    // Animate over 0.10 seconds
    double t = std::clamp(m_lnTimer / 0.10, 0.0, 1.0);

    // Ease-Out Cubic curve for a fast, clean stop without bouncing
    double t_m_1 = t - 1.0;
    double easeOut = 1.0 + (t_m_1 * t_m_1 * t_m_1);

    double currentPosition = positionStart * (1.0 - easeOut);

    m_lnLogo->Position2 = UDim2::fromOffset(0, currentPosition);
    m_lnLogo->Draw(delta);

    m_lnComboNum->Position2 = UDim2::fromOffset(0, currentPosition);
    m_lnComboNum->DrawNumber(std::get<9>(scores));

    m_lnTimer += delta;

    if (m_lnTimer > 1.0) {
      m_lnTimer = 0.0;
      m_drawLN = false;
    }
  }

  float gaugeVal =
      (float)m_game->GetScoreManager()->GetJamGauge() / kMaxJamGauge;
  if (gaugeVal > 0) {
    m_jamGauge->CalculateSize();

    int lerp;
    if (m_jamGauge->AbsoluteSize.Y > m_jamGauge->AbsoluteSize.X) {
      // Fill from bottom to top
      lerp = static_cast<int>(
          std::lerp(0.0, m_jamGauge->AbsoluteSize.Y, (double)gaugeVal));
      Rect rc = {
          (int)m_jamGauge->AbsolutePosition.X,
          (int)(m_jamGauge->AbsolutePosition.Y + m_jamGauge->AbsoluteSize.Y -
                lerp),
          (int)(m_jamGauge->AbsolutePosition.X + m_jamGauge->AbsoluteSize.X),
          (int)(m_jamGauge->AbsolutePosition.Y + m_jamGauge->AbsoluteSize.Y)};

      m_jamGauge->Draw(&rc);
    } else {
      // Fill from left to right
      lerp = static_cast<int>(
          std::lerp(0.0, m_jamGauge->AbsoluteSize.X, gaugeVal));
      Rect rc = {
          (int)m_jamGauge->AbsolutePosition.X,
          (int)m_jamGauge->AbsolutePosition.Y,
          (int)(m_jamGauge->AbsolutePosition.X + lerp),
          (int)(m_jamGauge->AbsolutePosition.Y + m_jamGauge->AbsoluteSize.Y)};

      m_jamGauge->Draw(&rc);
    }
  }

  float currentProgress =
      (float)m_game->GetAudioPosition() / (float)m_game->GetAudioLength();
  if (currentProgress > 0) {
    m_waveGage->CalculateSize();

    int min = 0, max = (int)m_waveGage->AbsoluteSize.X;
    int lerp = (int)std::lerp(min, max, currentProgress);

    Rect rc = {
        (int)m_waveGage->AbsolutePosition.X,
        (int)m_waveGage->AbsolutePosition.Y,
        (int)(m_waveGage->AbsolutePosition.X + lerp),
        (int)(m_waveGage->AbsolutePosition.Y + m_waveGage->AbsoluteSize.Y)};

    m_waveGage->Draw(&rc);
  }

  // Fix if playtime sometimes slighly double draw
  int PlayTime = 0;
  int currentMinutes = 0 / 60;
  int currentSeconds = 0 % 60;

  bool isPlaying = EnvironmentSetup::GetInt("NowPlaying") == 1;
  if (isPlaying) // get timer if on play state
  {
    PlayTime = std::clamp(m_game->GetPlayTime(), 0, INT_MAX);
    currentMinutes = PlayTime / 60;
    currentSeconds = PlayTime % 60;
  } else { // stop timer if end or fail
    int lastPlayTime = PlayTime;
    currentMinutes = lastPlayTime / 60;
    currentSeconds = lastPlayTime % 60;
  }

  m_minuteNum->DrawNumber(currentMinutes);
  m_secondNum->DrawNumber(currentSeconds);

  for (int i = 0; i < m_keyCount; i++) {
    if (EnvironmentSetup::GetInt("NowPlaying") == 1) {
      if (m_drawHit[i]) {
        m_hitEffect[i]->DrawOnce(delta);
      }

      if (m_drawHold[i]) {
        m_holdEffect[i]->Draw(delta);
      }
    } else {
      m_hitEffect[i]->SetIndex(m_hitEffect[i]->GetFrameCount() - 1);

      if (m_drawHold[i]) {
        m_holdEffect[i]->SetIndex(m_holdEffect[i]->GetFrameCount() - 1);
      }
    }
  }

  if (m_drawExitButton) {
    m_exitBtn->Draw();
  }

  auto titleStr = m_game->GetTitle();
  if (m_autoPlay) titleStr += (const char8_t*)" [Autoplay]";
  m_title->Draw(titleStr);

  m_gameInfo->Position = m_gameInfoPos;
  m_gameInfo->Draw(GAMEINFO_TEXT);

  if (m_autoPlay) {
    if (m_autoImage) {
      m_autoImage->Draw();
    } else {
      m_autoText->Position = m_autoTextPos;
      m_autoText->Draw(AUTOPLAY_TEXT);

      m_autoTextPos.X.Offset -= delta * 30.0;
      if (m_autoTextPos.X.Offset < (-m_autoTextSize + 30)) {
        m_autoTextPos =
            UDim2::fromOffset(GameWindow::GetInstance()->GetBufferWidth(), 60);
      }
    }
  }

  int idx = 2;
  try {
    idx = std::stoi(Configuration::Load("Game", "GuideLine"));
  } catch (const std::invalid_argument &) {
    idx = 2;
  }

  m_game->SetGuideLineIndex(idx);
}

void GameplayScene::Input(double delta) {
  if (m_resourceFucked)
    return;

  m_game->Input(delta);
}

void GameplayScene::OnKeyDown(const KeyState &state) {
  if (m_resourceFucked)
    return;

  m_game->OnKeyDown(state);
}

void GameplayScene::OnKeyUp(const KeyState &state) {
  if (m_resourceFucked)
    return;

  m_game->OnKeyUp(state);
}

void GameplayScene::OnMouseDown(const MouseState &state) {}

bool GameplayScene::Attach() {
  SkinManager::GetInstance()->ReloadSkin();

  m_ended = false;
  m_starting = false;
  m_doExit = false;
  m_drawExitButton = false;
  m_resourceFucked = false;
  m_drawJudge = false;
  lifeFillDuration = 0.0;
  m_autoPlay = EnvironmentSetup::GetInt("Autoplay") == 1;

  try {
    auto manager = SkinManager::GetInstance();

    int LaneOffset = 5;
    int HitPos = 480;

    try {
      LaneOffset = std::stoi(manager->GetSkinProp("Game", "LaneOffset", "5"));
      HitPos = std::stoi(manager->GetSkinProp("Game", "HitPos", "480"));
    } catch (const std::invalid_argument &) {
      throw std::runtime_error(
          "Invalid parameter on Skin::Game::LaneOffset or Skin::Game::HitPos");
    }

    int arena = EnvironmentSetup::GetInt("Arena");
    auto skinPath = manager->GetPath();
    auto playingPath = skinPath / "Playing";
    auto arenaPath = playingPath / "Arena";

    if (arena == 0 || arena == -1) {
      std::random_device dev;
      std::mt19937 rng(dev());

      int max_arena = SkinManager::GetInstance()->GetArenas().size() - 1;
      if (max_arena < 1)
        max_arena = 1;

      std::uniform_int_distribution<> dist(1, max_arena);

      arena = dist(rng);
    }

    // HACK: arena -1 is Song Background
    auto numberedArenaPath = arenaPath / std::to_string(arena);
    if (std::filesystem::exists(numberedArenaPath)) {
        arenaPath = numberedArenaPath;
    }

    EnvironmentSetup::SetInt("CurrentArena", arena);

    if (!std::filesystem::exists(arenaPath)) {
        arenaPath = playingPath;
    }
    Chart *songChart = (Chart *)EnvironmentSetup::GetObj("SONG");
    m_keyCount = songChart ? songChart->m_keyCount : 7;
    manager->SetKeyCount(m_keyCount);
    // Arena Index logic is removed since Arena.ini is merged into Playing.ini

    m_keyState.clear();
    m_keyButtons.clear();
    m_keyDowns.clear();
    m_keyLighting.clear();
    m_hitEffect.clear();
    m_holdEffect.clear();

    for (int i = 0; i < 7; i++) {
        m_drawHit[i] = false;
        m_drawHold[i] = false;
    }

    for (int i = 0; i < m_keyCount; i++) {
      m_keyState[i] = false;
    }

    m_title = std::make_unique<Text>(13);
    auto TitlePos = manager->GetPosition(SkinGroup::Playing,
                                         "Title"); // conf.GetPosition("Title");
    auto RectPos =
        manager->GetRect(SkinGroup::Playing, "Title"); // conf.GetRect("Title");
    m_title->Position = UDim2::fromOffset(TitlePos[0].X, TitlePos[0].Y);
    m_title->AnchorPoint = {TitlePos[0].AnchorPointX, TitlePos[0].AnchorPointY};
    m_title->Clip = {RectPos[0].X, RectPos[0].Y, RectPos[0].Width,
                     RectPos[0].Height};

    m_autoText = std::make_unique<Text>(12);
    m_autoTextSize = m_autoText->CalculateSize(AUTOPLAY_TEXT);
    m_autoTextPos =
        UDim2::fromOffset(GameWindow::GetInstance()->GetBufferWidth(), 50);

    auto autoplayImgPath = EnvironmentSetup::GetPath("GamePath") / "Resources" / "Mods" / "Autoplay.png";
    if (std::filesystem::exists(autoplayImgPath)) {
        m_autoImage = std::make_unique<Texture2D>(autoplayImgPath);
    }

    m_gameInfo = std::make_unique<Text>(8);
    m_gameInfoSize = m_gameInfo->CalculateSize(GAMEINFO_TEXT);
    m_gameInfoPos =
        UDim2::fromOffset(/*GameWindow::GetInstance()->GetBufferWidth()*/ 0,
                          GameWindow::GetInstance()->GetBufferHeight() - 10);

    std::filesystem::path playBgPath = arenaPath / "PlayingBG.png";
    bool hasBg = true;
    if (!std::filesystem::exists(playBgPath)) {
        Chart *chartCheck = (Chart *)EnvironmentSetup::GetObj("SONG");
        if (chartCheck && !chartCheck->m_backgroundFile.empty()) {
            playBgPath = chartCheck->m_beatmapDirectory / chartCheck->m_backgroundFile;
            if (!std::filesystem::exists(playBgPath)) {
                hasBg = false;
            }
        } else {
            hasBg = false;
        }
    }

    if (hasBg) {
        m_PlayBG = std::make_unique<Texture2D>(playBgPath);
        auto PlayBGPos = manager->GetPosition(SkinGroup::Playing, "PlayingBG");
        m_PlayBG->Position = UDim2::fromOffset(PlayBGPos[0].X, PlayBGPos[0].Y);
        m_PlayBG->AnchorPoint = {PlayBGPos[0].AnchorPointX,
                                 PlayBGPos[0].AnchorPointY};
    } else {
        m_PlayBG = nullptr;
    }

    auto parsePositionsFallback = [&](const std::string& propName, std::vector<PositionValue>& target) {
        std::string propStr = manager->GetSkinConfigProp(SkinGroup::Playing, "Key#" + std::to_string(m_keyCount), propName);
        if (!propStr.empty()) {
            target.clear();
            auto splits = splitString(propStr, '|');
            for (auto& s : splits) {
                auto coords = splitString(s, ',');
                PositionValue pv = {0, 0, 0.0f, 0.0f};
                if (coords.size() >= 1) pv.X = std::stoi(coords[0]);
                if (coords.size() >= 2) pv.Y = std::stoi(coords[1]);
                if (coords.size() >= 3) pv.AnchorPointX = std::stof(coords[2]);
                if (coords.size() >= 4) pv.AnchorPointY = std::stof(coords[3]);
                target.push_back(pv);
            }
        }
    };

    std::vector<PositionValue> conKeyLight;
    try { conKeyLight = manager->GetPosition(SkinGroup::Playing, "KeyLighting"); } catch (...) {}
    parsePositionsFallback("KeyLighting", conKeyLight);
    
    std::vector<PositionValue> conKeyButton;
    try { conKeyButton = manager->GetPosition(SkinGroup::Playing, "KeyButton"); } catch (...) {}
    parsePositionsFallback("KeyButton", conKeyButton);
    
    std::vector<PositionValue> conHitEffect;
    try { conHitEffect = manager->GetPosition(SkinGroup::Playing, "HitEffect"); } catch (...) {}
    parsePositionsFallback("HitEffect", conHitEffect);
    
    std::vector<PositionValue> conHoldEffect;
    try { conHoldEffect = manager->GetPosition(SkinGroup::Playing, "HoldEffect"); } catch (...) {}
    parsePositionsFallback("HoldEffect", conHoldEffect);
    
    std::string colStartStr = manager->GetSkinConfigProp(SkinGroup::Playing, "Key#" + std::to_string(m_keyCount), "ColumnStart");
    std::string colWidthStr = manager->GetSkinConfigProp(SkinGroup::Playing, "Key#" + std::to_string(m_keyCount), "ColumnWidth");
    std::string noteHeightStr = manager->GetSkinConfigProp(SkinGroup::Playing, "Key#" + std::to_string(m_keyCount), "NoteHeight");
    std::string keyOffsetStr = manager->GetSkinConfigProp(SkinGroup::Playing, "Key#" + std::to_string(m_keyCount), "KeyOffset");

    std::vector<int> colWidths;
    int colStart = 0;
    int keyOffset = 0;
    bool useManiaLayout = false;
    
    if (!keyOffsetStr.empty()) {
        keyOffset = std::stoi(keyOffsetStr);
    }
    
    bool genKeyButton = conKeyButton.size() < m_keyCount;
    bool genKeyLight = conKeyLight.size() < m_keyCount;
    bool genHitEffect = conHitEffect.size() < m_keyCount;
    bool genHoldEffect = conHoldEffect.size() < m_keyCount;

    if (!colStartStr.empty() && !colWidthStr.empty()) {
        colStart = std::stoi(colStartStr);
        auto widthSplits = splitString(colWidthStr, ',');
        for (auto& w : widthSplits) {
            colWidths.push_back(std::stoi(w));
        }
        if (colWidths.size() == 1) {
            int w = colWidths[0];
            for (int i = 1; i < m_keyCount; i++) colWidths.push_back(w);
        }
        
        if (colWidths.size() >= m_keyCount) {
            useManiaLayout = true;
            if (genKeyLight) conKeyLight.clear();
            if (genKeyButton) conKeyButton.clear();
            if (genHitEffect) conHitEffect.clear();
            if (genHoldEffect) conHoldEffect.clear();
            
            int currentX = colStart;
            for (int i = 0; i < m_keyCount; i++) {
                PositionValue btnPos = { currentX, HitPos, 0.0f, 0.0f };
                PositionValue lgtPos = { currentX, HitPos, 0.0f, 1.0f };
                
                if (genKeyButton) conKeyButton.push_back(btnPos);
                if (genKeyLight) conKeyLight.push_back(lgtPos);
                if (genHitEffect) conHitEffect.push_back(btnPos);
                if (genHoldEffect) conHoldEffect.push_back(btnPos);
                
                currentX += colWidths[i];
            }
        }
    }

    if (!useManiaLayout && conKeyButton.size() < m_keyCount) {
      throw std::runtime_error(
          "Playing.ini : Positions : KeyButton : Not enough "
          "positions! (count < " + std::to_string(m_keyCount) + ")");
    }

    std::string playfieldImg = manager->GetSkinConfigProp(SkinGroup::Playing, "Key#" + std::to_string(m_keyCount), "Playfield");
    if (playfieldImg.empty()) playfieldImg = manager->GetImageMapping(SkinGroup::Playing, "Playfield");
    if (playfieldImg.empty()) playfieldImg = "Playfield";

    auto playfieldPos = manager->GetPosition(SkinGroup::Playing, "Playfield");
    m_Playfield = std::make_unique<Texture2D>(playingPath / (playfieldImg + ".png"));
    m_Playfield->Position =
        UDim2::fromOffset(playfieldPos[0].X, playfieldPos[0].Y);
    m_Playfield->AnchorPoint = {playfieldPos[0].AnchorPointX,
                                playfieldPos[0].AnchorPointY};

    std::string imageFlipStr = manager->GetImageMapping(SkinGroup::Playing, "ImageFlip");
    std::vector<bool> imageFlip;
    if (!imageFlipStr.empty()) {
        auto splits = splitString(imageFlipStr, '|');
        for (auto& s : splits) {
            imageFlip.push_back(s == "1" || s == "true" || s == "TRUE");
        }
    } else {
        imageFlip = {false, false, false, false, false, false, false};
    }

    std::string imageAlignStr = manager->GetImageMapping(SkinGroup::Playing, "ImageAlignment");
    std::vector<char> imageAlign;
    if (!imageAlignStr.empty()) {
        auto splits = splitString(imageAlignStr, '|');
        for (auto& s : splits) {
            if (s.length() > 0) imageAlign.push_back(toupper(s[0]));
            else imageAlign.push_back('C');
        }
    } else {
        imageAlign = std::vector<char>(m_keyCount, 'C');
    }

    std::string noStretchStr = manager->GetImageMapping(SkinGroup::Playing, "NoStretchToColumn");
    std::transform(noStretchStr.begin(), noStretchStr.end(), noStretchStr.begin(), ::tolower);
    bool autoWidth = !(noStretchStr == "1" || noStretchStr == "true");

    std::vector<std::string> keyMap;
    switch (m_keyCount) {
        case 4: keyMap = { "1", "2", "2", "1" }; break;
        case 5: keyMap = { "1", "2", "S", "2", "1" }; break;
        case 6: keyMap = { "1", "2", "1", "1", "2", "1" }; break;
        case 7: keyMap = { "1", "2", "1", "S", "1", "2", "1" }; break;
        default: 
            for(int i=0; i<m_keyCount; i++) keyMap.push_back("1"); 
            break;
    }

    for (int i = 0; i < m_keyCount; i++) {
        std::string btnBase = manager->GetSkinConfigProp(SkinGroup::Playing, "Key#" + std::to_string(m_keyCount), "KeyButton" + std::to_string(i));
        if (btnBase.empty()) btnBase = manager->GetImageMapping(SkinGroup::Playing, "KeyButton" + std::to_string(i));
        if (btnBase.empty()) btnBase = "KeyButton" + keyMap[i];

        std::string dwnBase = manager->GetSkinConfigProp(SkinGroup::Playing, "Key#" + std::to_string(m_keyCount), "KeyDown" + std::to_string(i));
        if (dwnBase.empty()) dwnBase = manager->GetImageMapping(SkinGroup::Playing, "KeyDown" + std::to_string(i));
        if (dwnBase.empty()) dwnBase = "KeyDown" + keyMap[i];

        std::string lgtBase = manager->GetSkinConfigProp(SkinGroup::Playing, "Key#" + std::to_string(m_keyCount), "KeyLighting" + std::to_string(i));
        if (lgtBase.empty()) lgtBase = manager->GetImageMapping(SkinGroup::Playing, "KeyLighting" + std::to_string(i));
        if (lgtBase.empty()) lgtBase = "KeyLighting" + keyMap[i];

        std::vector<std::filesystem::path> btnPaths;
        std::vector<std::filesystem::path> dwnPaths;
        std::vector<std::filesystem::path> lgtPaths;

        if (std::filesystem::exists(playingPath / (btnBase + "-0.png"))) {
            int frame = 0;
            while (std::filesystem::exists(playingPath / (btnBase + "-" + std::to_string(frame) + ".png"))) {
                btnPaths.push_back(playingPath / (btnBase + "-" + std::to_string(frame) + ".png"));
                frame++;
            }
        } else {
            btnPaths.push_back(playingPath / (btnBase + ".png"));
        }

        if (std::filesystem::exists(playingPath / (dwnBase + "-0.png"))) {
            int frame = 0;
            while (std::filesystem::exists(playingPath / (dwnBase + "-" + std::to_string(frame) + ".png"))) {
                dwnPaths.push_back(playingPath / (dwnBase + "-" + std::to_string(frame) + ".png"));
                frame++;
            }
        } else if (std::filesystem::exists(playingPath / (dwnBase + ".png"))) {
            dwnPaths.push_back(playingPath / (dwnBase + ".png"));
        } else {
            dwnPaths = btnPaths; // Fallback to KeyButton if KeyDown doesn't exist
        }

        if (std::filesystem::exists(playingPath / (lgtBase + "-0.png"))) {
            int frame = 0;
            while (std::filesystem::exists(playingPath / (lgtBase + "-" + std::to_string(frame) + ".png"))) {
                lgtPaths.push_back(playingPath / (lgtBase + "-" + std::to_string(frame) + ".png"));
                frame++;
            }
        } else {
            lgtPaths.push_back(playingPath / (lgtBase + ".png"));
        }

        m_keyButtons[i] = std::make_unique<Sprite2D>(btnPaths, 0.016f);
        m_keyDowns[i] = std::make_unique<Sprite2D>(dwnPaths, 0.016f);
        m_keyLighting[i] = std::make_unique<Sprite2D>(lgtPaths, 0.016f);

        if (i < imageFlip.size() && imageFlip[i]) {
            m_keyButtons[i]->FlipX = true;
            m_keyDowns[i]->FlipX = true;
            m_keyLighting[i]->FlipX = true;
        }

        PositionValue lgtPos = conKeyLight.size() > i ? conKeyLight[i] : PositionValue{conKeyButton.size() > i ? conKeyButton[i].X : 0.0, static_cast<double>(HitPos), 0.0f, 1.0f};
        m_keyLighting[i]->Position = UDim2::fromOffset(lgtPos.X, lgtPos.Y);
        m_keyLighting[i]->AnchorPoint = {lgtPos.AnchorPointX, lgtPos.AnchorPointY};

        PositionValue btnPos = conKeyButton.size() > i ? conKeyButton[i] : PositionValue{0.0, static_cast<double>(HitPos), 0.0f, 0.0f};
        m_keyButtons[i]->Position = UDim2::fromOffset(btnPos.X, btnPos.Y);
        m_keyButtons[i]->AnchorPoint = {btnPos.AnchorPointX, btnPos.AnchorPointY};
        
        PositionValue downPos = conKeyButton[i];
        try {
            auto conKeyDown = manager->GetPosition(SkinGroup::Playing, "KeyDown");
            parsePositionsFallback("KeyDown", conKeyDown);
            if (conKeyDown.size() > i) {
                downPos = conKeyDown[i];
                m_keyDowns[i]->Position = UDim2::fromOffset(conKeyDown[i].X, conKeyDown[i].Y);
                m_keyDowns[i]->AnchorPoint = {conKeyDown[i].AnchorPointX, conKeyDown[i].AnchorPointY};
            } else {
                throw std::runtime_error("Not enough KeyDown");
            }
        } catch (...) {
            m_keyDowns[i]->Position = m_keyButtons[i]->Position;
            m_keyDowns[i]->AnchorPoint = m_keyButtons[i]->AnchorPoint;
        }

        if (useManiaLayout) {
            bool isFlipped = false;
            if (i < imageFlip.size() && imageFlip[i]) {
                isFlipped = true;
            }

            auto alignSprite = [&](std::shared_ptr<Sprite2D>& sprite, PositionValue& posDef, bool scaleWidth) {
                if (sprite && sprite->GetTexture()) {
                    int origWidth = sprite->GetTexture()->GetOriginalRECT().right;
                    int origHeight = sprite->GetTexture()->GetOriginalRECT().bottom;
                    
                    int displayWidth = origWidth;
                    if (scaleWidth) {
                        displayWidth = colWidths[i];
                    }
                    sprite->Size = UDim2::fromOffset(displayWidth, origHeight);
                    
                    int currentX = posDef.X;
                    char align = 'C';
                    if (i < imageAlign.size()) align = imageAlign[i];

                    if (align == 'L') {
                        sprite->Position = UDim2::fromOffset(currentX, posDef.Y + keyOffset);
                    } else if (align == 'R') {
                        sprite->Position = UDim2::fromOffset(currentX + colWidths[i] - displayWidth, posDef.Y + keyOffset);
                    } else { // Center
                        sprite->Position = UDim2::fromOffset(currentX + (colWidths[i] - displayWidth) / 2.0f, posDef.Y + keyOffset);
                    }
                    sprite->AnchorPoint = {posDef.AnchorPointX, posDef.AnchorPointY};
                }
            };

            bool scale = autoWidth;
            if (genKeyLight) alignSprite(m_keyLighting[i], conKeyLight[i], scale);
            if (genKeyButton) {
                alignSprite(m_keyButtons[i], conKeyButton[i], scale);
                alignSprite(m_keyDowns[i], downPos, scale);
            }
        }
    }

    std::string comboNumImg = manager->GetSkinConfigProp(SkinGroup::Playing, "Key#" + std::to_string(m_keyCount), "ComboNum");
    if (comboNumImg.empty()) comboNumImg = manager->GetImageMapping(SkinGroup::Playing, "ComboNum");
    if (comboNumImg.empty()) comboNumImg = "ComboNum";

    std::vector<std::filesystem::path> numComboPaths = {};
    for (int i = 0; i < 10; i++) {
      auto filePath = arenaPath / (comboNumImg + std::to_string(i) + ".png");

      if (!CheckSkinComponent(filePath)) {
        std::cout << "Missing: " << filePath.filename() << std::endl;

        throw std::runtime_error("Failed to load Integer Images 0-9, please check your skin folder.");
      }

      numComboPaths.emplace_back(filePath);
    }

    m_comboNum = std::make_unique<NumericTexture>(numComboPaths);
    auto numPos = manager->GetNumeric(SkinGroup::Playing, "Combo").front();

    m_comboNum->Position = UDim2::fromOffset(numPos.X, numPos.Y);
    m_comboNum->NumberPosition = IntToPos(numPos.Direction);
    m_comboNum->MaxDigits = numPos.MaxDigit;
    m_comboNum->FillWithZeros = numPos.FillWithZero;
    m_comboNum->AlphaBlend = true;

    std::string jamNumImg = manager->GetSkinConfigProp(SkinGroup::Playing, "Key#" + std::to_string(m_keyCount), "JamNum");
    if (jamNumImg.empty()) jamNumImg = manager->GetImageMapping(SkinGroup::Playing, "JamNum");
    if (jamNumImg.empty()) jamNumImg = "JamNum";

    std::vector<std::filesystem::path> numJamPaths = {};
    for (int i = 0; i < 10; i++) {
      auto filePath = playingPath / (jamNumImg + std::to_string(i) + ".png");

      if (!CheckSkinComponent(filePath)) {
        std::cout << "Missing: " << filePath.filename() << std::endl;

        throw std::runtime_error("Failed to load Integer Images 0-9, please check your skin folder.");
      }

      numJamPaths.emplace_back(filePath);
    }

    m_jamNum = std::make_unique<NumericTexture>(numJamPaths);
    numPos = manager->GetNumeric(SkinGroup::Playing, "Jam").front();

    m_jamNum->Position = UDim2::fromOffset(numPos.X, numPos.Y);
    m_jamNum->NumberPosition = IntToPos(numPos.Direction);
    m_jamNum->MaxDigits = numPos.MaxDigit;
    m_jamNum->FillWithZeros = numPos.FillWithZero;

    std::string scoreNumImg = manager->GetSkinConfigProp(SkinGroup::Playing, "Key#" + std::to_string(m_keyCount), "ScoreNum");
    if (scoreNumImg.empty()) scoreNumImg = manager->GetImageMapping(SkinGroup::Playing, "ScoreNum");
    if (scoreNumImg.empty()) scoreNumImg = "ScoreNum";

    std::vector<std::filesystem::path> numScorePaths = {};
    for (int i = 0; i < 10; i++) {
      auto filePath = playingPath / (scoreNumImg + std::to_string(i) + ".png");

      if (!CheckSkinComponent(filePath)) {
        std::cout << "Missing: " << filePath.filename() << std::endl;

        throw std::runtime_error("Failed to load Integer Images 0-9, please "
                                 "check your skin folder.");
      }

      numScorePaths.emplace_back(filePath);
    }

    m_scoreNum = std::make_unique<NumericTexture>(numScorePaths);
    numPos = manager->GetNumeric(SkinGroup::Playing, "Score")
                 .front(); // conf.GetNumeric("Score").front();

    m_scoreNum->Position = UDim2::fromOffset(numPos.X, numPos.Y);
    m_scoreNum->NumberPosition = IntToPos(numPos.Direction);
    m_scoreNum->MaxDigits = numPos.MaxDigit;
    m_scoreNum->FillWithZeros = numPos.FillWithZero;

    std::vector<std::string> judgeFileName = {"Miss", "Bad", "Good", "Cool"};
    auto judgePos =
        manager->GetPosition(SkinGroup::Playing, "Judge");
    if (judgePos.size() < 1) {
      throw std::runtime_error("Playing.ini : Positions : Judge : Not enough "
                               "arguments, expected 1 or 4");
    }

    std::string judgeImg = manager->GetSkinConfigProp(SkinGroup::Playing, "Key#" + std::to_string(m_keyCount), "Judge");
    if (judgeImg.empty()) judgeImg = manager->GetImageMapping(SkinGroup::Playing, "Judge");
    if (judgeImg.empty()) judgeImg = "Judge";

    for (int i = 0; i < 4; i++) {
      auto filePathBase = arenaPath / (judgeImg + judgeFileName[i]);
      
      int posIndex = (judgePos.size() >= 4) ? i : 0;

      if (std::filesystem::exists(filePathBase.string() + "0.png") || std::filesystem::exists(filePathBase.string() + "-0.png")) {
          // Load Animated Sequence
          std::vector<std::filesystem::path> animPaths;
          int frame = 0;
          
          while (std::filesystem::exists(filePathBase.string() + std::to_string(frame) + ".png") ||
                 std::filesystem::exists(filePathBase.string() + "-" + std::to_string(frame) + ".png")) {
              
              if (std::filesystem::exists(filePathBase.string() + std::to_string(frame) + ".png")) {
                  animPaths.push_back(filePathBase.string() + std::to_string(frame) + ".png");
              } else {
                  animPaths.push_back(filePathBase.string() + "-" + std::to_string(frame) + ".png");
              }
              frame++;
          }

          float frameTime = 0.02f;
          if (judgePos[posIndex].FPS > 0.0f) {
              frameTime = 1.0f / judgePos[posIndex].FPS;
          }

          m_judgementSprite[i] = std::make_unique<Sprite2D>(animPaths, frameTime);
          m_judgementSprite[i]->Position = UDim2::fromOffset(judgePos[posIndex].X, judgePos[posIndex].Y);
          m_judgementSprite[i]->AlphaBlend = true;
      } else {
          auto filePath = arenaPath / (judgeImg + judgeFileName[i] + ".png");

          if (!std::filesystem::exists(filePath)) {
            throw std::runtime_error("Failed to load Judge image!");
          }

          m_judgement[i] = std::move(std::make_unique<Texture2D>(filePath));
          m_judgement[i]->Position =
              UDim2::fromOffset(judgePos[posIndex].X, judgePos[posIndex].Y);
          m_judgement[i]->AlphaBlend = true;
      }
    }

    std::string jamGaugeImg = manager->GetSkinConfigProp(SkinGroup::Playing, "Key#" + std::to_string(m_keyCount), "JamGauge");
    if (jamGaugeImg.empty()) jamGaugeImg = manager->GetImageMapping(SkinGroup::Playing, "JamGauge");
    if (jamGaugeImg.empty()) jamGaugeImg = "JamGauge";
    m_jamGauge = std::make_unique<Texture2D>(playingPath / (jamGaugeImg + ".png"));
    auto gaugePos = manager->GetPosition(
        SkinGroup::Playing, "JamGauge"); // conf.GetPosition("JamGauge");
    if (gaugePos.size() < 1) {
      throw std::runtime_error(
          "Playing.ini : Positions : JamGauge : Position Not defined!");
    }

    m_jamGauge->Position = UDim2::fromOffset(gaugePos[0].X, gaugePos[0].Y);
    m_jamGauge->AnchorPoint = {gaugePos[0].AnchorPointX,
                               gaugePos[0].AnchorPointY};

    auto jamLogoPos = manager->GetSprite(
        SkinGroup::Playing, "JamLogo"); // conf.GetSprite("JamLogo");
    std::vector<std::filesystem::path> jamLogoFileName = {};
    std::string jamLogoImg = manager->GetSkinConfigProp(SkinGroup::Playing, "Key#" + std::to_string(m_keyCount), "JamLogo");
    if (jamLogoImg.empty()) jamLogoImg = manager->GetImageMapping(SkinGroup::Playing, "JamLogo");
    if (jamLogoImg.empty()) jamLogoImg = "JamLogo";

    if (std::filesystem::exists(playingPath / (jamLogoImg + "-0.png"))) {
        int frame = 0;
        while (std::filesystem::exists(playingPath / (jamLogoImg + "-" + std::to_string(frame) + ".png"))) {
            jamLogoFileName.emplace_back(playingPath / (jamLogoImg + "-" + std::to_string(frame) + ".png"));
            frame++;
        }
    } else if (std::filesystem::exists(playingPath / (jamLogoImg + "0.png"))) {
        int frame = 0;
        while (std::filesystem::exists(playingPath / (jamLogoImg + std::to_string(frame) + ".png"))) {
            jamLogoFileName.emplace_back(playingPath / (jamLogoImg + std::to_string(frame) + ".png"));
            frame++;
        }
    } else if (std::filesystem::exists(playingPath / (jamLogoImg + ".png"))) {
        jamLogoFileName.emplace_back(playingPath / (jamLogoImg + ".png"));
    } else {
        throw std::runtime_error("Failed to load Jam Logo image!");
    }

    m_jamLogo = std::make_unique<Sprite2D>(jamLogoFileName, 0.25f);
    m_jamLogo->Position = UDim2::fromOffset(jamLogoPos.X, jamLogoPos.Y);
    m_jamLogo->AnchorPoint = {(double)jamLogoPos.AnchorPointX,
                              (double)jamLogoPos.AnchorPointY};
    m_jamLogo->SetFPS(jamLogoPos.FrameTime);

    auto lifeBarPos = manager->GetSprite(
        SkinGroup::Playing, "LifeBar"); // conf.GetSprite("LifeBar");
    std::vector<std::filesystem::path> lifeBarFileName = {};
    std::string lifeBarImg = manager->GetSkinConfigProp(SkinGroup::Playing, "Key#" + std::to_string(m_keyCount), "LifeBar");
    if (lifeBarImg.empty()) lifeBarImg = manager->GetImageMapping(SkinGroup::Playing, "LifeBar");
    if (lifeBarImg.empty()) lifeBarImg = "LifeBar";

    if (std::filesystem::exists(playingPath / (lifeBarImg + "-0.png"))) {
        int frame = 0;
        while (std::filesystem::exists(playingPath / (lifeBarImg + "-" + std::to_string(frame) + ".png"))) {
            lifeBarFileName.emplace_back(playingPath / (lifeBarImg + "-" + std::to_string(frame) + ".png"));
            frame++;
        }
    } else if (std::filesystem::exists(playingPath / (lifeBarImg + "0.png"))) {
        int frame = 0;
        while (std::filesystem::exists(playingPath / (lifeBarImg + std::to_string(frame) + ".png"))) {
            lifeBarFileName.emplace_back(playingPath / (lifeBarImg + std::to_string(frame) + ".png"));
            frame++;
        }
    } else if (std::filesystem::exists(playingPath / (lifeBarImg + ".png"))) {
        lifeBarFileName.emplace_back(playingPath / (lifeBarImg + ".png"));
    } else {
        throw std::runtime_error("Failed to load Life Bar image!");
    }

    m_lifeBar = std::make_unique<Sprite2D>(lifeBarFileName, 0.15f);
    m_lifeBar->Position = UDim2::fromOffset(lifeBarPos.X, lifeBarPos.Y);
    m_lifeBar->AnchorPoint = {lifeBarPos.AnchorPointX, lifeBarPos.AnchorPointY};
    m_lifeBar->SetFPS(0.0);

    std::string longNoteNumImg = manager->GetSkinConfigProp(SkinGroup::Playing, "Key#" + std::to_string(m_keyCount), "LongNoteNum");
    if (longNoteNumImg.empty()) longNoteNumImg = manager->GetImageMapping(SkinGroup::Playing, "LongNoteNum");
    if (longNoteNumImg.empty()) longNoteNumImg = "LongNoteNum";

    std::vector<std::filesystem::path> lnComboFileName = {};
    for (int i = 0; i < 10; i++) {
      auto filePath =
          playingPath / (longNoteNumImg + std::to_string(i) + ".png");

      if (!CheckSkinComponent(filePath)) {
        std::cout << "Missing: " << filePath.filename() << std::endl;
        throw std::runtime_error("Failed to load Long Note Combo image!");
      }

      lnComboFileName.emplace_back(filePath);
    }

    std::string statsNumImg = manager->GetSkinConfigProp(SkinGroup::Playing, "Key#" + std::to_string(m_keyCount), "StatsNum");
    if (statsNumImg.empty()) statsNumImg = manager->GetImageMapping(SkinGroup::Playing, "StatsNum");
    if (statsNumImg.empty()) statsNumImg = "StatsNum";

    std::vector<std::filesystem::path> statsNumFileName = {};
    for (int i = 0; i < 10; i++) {
      auto filePath = playingPath / (statsNumImg + std::to_string(i) + ".png");

      if (!CheckSkinComponent(filePath)) {
        std::cout << "Missing: " << filePath.filename() << std::endl;
        throw std::runtime_error("Failed to load Stats Number image!");
      }

      statsNumFileName.emplace_back(filePath);
    }

    m_statsNum = std::make_unique<NumericTexture>(statsNumFileName);
    auto statsNumPos = manager->GetNumeric(
        SkinGroup::Playing, "Stats"); // conf.GetNumeric("Stats");
    if (statsNumPos.size() < 5) {
      throw std::runtime_error(
          "Playing.ini : Numerics : Stats : Not enough positions! (count < 5)");
    }

    m_statsNum->NumberPosition = IntToPos(statsNumPos[0].Direction);
    m_statsNum->MaxDigits = statsNumPos[0].MaxDigit;
    m_statsNum->FillWithZeros = statsNumPos[0].FillWithZero;
    m_statsNum->NumberPosition = IntToPos(statsNumPos[0].Direction);
    m_statsNum->AnchorPoint = {0, .5};

    // COOL: 0 -> MAXCOMBO: 4
    for (int i = 0; i < 5; i++) {
      m_statsPos[i] = UDim2::fromOffset(statsNumPos[i].X, statsNumPos[i].Y);
    }

    m_lnComboNum = std::make_unique<NumericTexture>(lnComboFileName);
    auto lnComboPos = manager->GetNumeric(
        SkinGroup::Playing,
        "LongNoteCombo"); // conf.GetNumeric("LongNoteCombo");
    if (lnComboPos.size() < 1) {
      throw std::runtime_error(
          "Playing.ini : Numerics : LongNoteCombo : Position Not defined!");
    }

    auto btnExitPos = manager->GetPosition(
        SkinGroup::Playing, "ExitButton"); // conf.GetPosition("ExitButton");
    auto btnExitRect =
        manager->GetRect(SkinGroup::Playing, "Exit"); // conf.GetRect("Exit");

    if (btnExitPos.size() < 1 || btnExitRect.size() < 1) {
      throw std::runtime_error(
          "Playing.ini : Positions|Rect : Exit : Not defined!");
    }

    std::string exitImg = manager->GetSkinConfigProp(SkinGroup::Playing, "Key#" + std::to_string(m_keyCount), "Exit");
    if (exitImg.empty()) exitImg = manager->GetImageMapping(SkinGroup::Playing, "Exit");
    if (exitImg.empty()) exitImg = "Exit";
    m_exitBtn = std::make_unique<Texture2D>(playingPath / (exitImg + ".png"));
    m_exitBtn->Position = UDim2::fromOffset(
        btnExitRect[0].X,
        btnExitRect[0].Y); // Fix Exit not functional with Playing.ini
    m_exitBtn->AnchorPoint = {btnExitPos[0].AnchorPointX,
                              btnExitPos[0].AnchorPointY};

    auto OnButtonHover = [&](int state) { m_drawExitButton = state; };

    auto OnButtonClick = [&]() { m_doExit = true; };

    m_exitButtonFunc =
        std::make_unique<Button>(btnExitRect[0].X, btnExitRect[0].Y,
                                 btnExitRect[0].Width, btnExitRect[0].Height);
    m_exitButtonFunc->OnMouseClick = OnButtonClick;
    m_exitButtonFunc->OnMouseHover = OnButtonHover;

    m_lnComboNum->Position =
        UDim2::fromOffset(lnComboPos[0].X, lnComboPos[0].Y);
    m_lnComboNum->NumberPosition = IntToPos(lnComboPos[0].Direction);
    m_lnComboNum->MaxDigits = lnComboPos[0].MaxDigit;
    m_lnComboNum->FillWithZeros = lnComboPos[0].FillWithZero;
    m_lnComboNum->AlphaBlend = true;

    auto lnLogoPos = manager->GetSprite(
        SkinGroup::Playing, "LongNoteLogo"); // conf.GetSprite("LongNoteLogo");
    std::vector<std::filesystem::path> lnLogoFileName = {};
    std::string lnLogoImg = manager->GetSkinConfigProp(SkinGroup::Playing, "Key#" + std::to_string(m_keyCount), "LongNoteLogo");
    if (lnLogoImg.empty()) lnLogoImg = manager->GetImageMapping(SkinGroup::Playing, "LongNoteLogo");
    if (lnLogoImg.empty()) lnLogoImg = "LongNoteLogo";

    if (std::filesystem::exists(playingPath / (lnLogoImg + "-0.png"))) {
        int frame = 0;
        while (std::filesystem::exists(playingPath / (lnLogoImg + "-" + std::to_string(frame) + ".png"))) {
            lnLogoFileName.emplace_back(playingPath / (lnLogoImg + "-" + std::to_string(frame) + ".png"));
            frame++;
        }
    } else if (std::filesystem::exists(playingPath / (lnLogoImg + "0.png"))) {
        int frame = 0;
        while (std::filesystem::exists(playingPath / (lnLogoImg + std::to_string(frame) + ".png"))) {
            lnLogoFileName.emplace_back(playingPath / (lnLogoImg + std::to_string(frame) + ".png"));
            frame++;
        }
    } else if (std::filesystem::exists(playingPath / (lnLogoImg + ".png"))) {
        lnLogoFileName.emplace_back(playingPath / (lnLogoImg + ".png"));
    } else {
        throw std::runtime_error("Failed to load Long Note Logo image!");
    }

    m_lnLogo = std::make_unique<Sprite2D>(lnLogoFileName, 0.25f);
    m_lnLogo->Position = UDim2::fromOffset(lnLogoPos.X, lnLogoPos.Y);
    m_lnLogo->AnchorPoint = {lnLogoPos.AnchorPointX, lnLogoPos.AnchorPointY};
    m_lnLogo->SetFPS(lnLogoPos.FrameTime);
    m_lnLogo->AlphaBlend = true;

    auto comboLogoPos = manager->GetSprite(SkinGroup::Playing, "ComboLogo");
    std::vector<std::filesystem::path> comboFileName = {};
    std::string comboLogoImg = manager->GetSkinConfigProp(SkinGroup::Playing, "Key#" + std::to_string(m_keyCount), "ComboLogo");
    if (comboLogoImg.empty()) comboLogoImg = manager->GetImageMapping(SkinGroup::Playing, "ComboLogo");
    if (comboLogoImg.empty()) comboLogoImg = "ComboLogo";

    if (std::filesystem::exists(arenaPath / (comboLogoImg + "-0.png"))) {
        int frame = 0;
        while (std::filesystem::exists(arenaPath / (comboLogoImg + "-" + std::to_string(frame) + ".png"))) {
            comboFileName.emplace_back(arenaPath / (comboLogoImg + "-" + std::to_string(frame) + ".png"));
            frame++;
        }
    } else if (std::filesystem::exists(arenaPath / (comboLogoImg + "0.png"))) {
        int frame = 0;
        while (std::filesystem::exists(arenaPath / (comboLogoImg + std::to_string(frame) + ".png"))) {
            comboFileName.emplace_back(arenaPath / (comboLogoImg + std::to_string(frame) + ".png"));
            frame++;
        }
    } else if (std::filesystem::exists(arenaPath / (comboLogoImg + ".png"))) {
        comboFileName.emplace_back(arenaPath / (comboLogoImg + ".png"));
    } else {
        // Some arenas might not have a ComboLogo
    }

    m_comboLogo = std::make_unique<Sprite2D>(comboFileName, 0.25f);
    m_comboLogo->Position = UDim2::fromOffset(comboLogoPos.X, comboLogoPos.Y);
    m_comboLogo->AnchorPoint = {comboLogoPos.AnchorPointX,
                                comboLogoPos.AnchorPointY};
    m_comboLogo->SetFPS(comboLogoPos.FrameTime);
    m_comboLogo->AlphaBlend = true;

    m_waveGage = std::make_unique<Texture2D>(playingPath / "WaveGage.png");
    auto waveGagePos = manager->GetPosition(SkinGroup::Playing, "WaveGage")
                           .front(); // conf.GetPosition("WaveGage").front();
    m_waveGage->Position = UDim2::fromOffset(waveGagePos.X, waveGagePos.Y);
    m_waveGage->AnchorPoint = {waveGagePos.AnchorPointX,
                               waveGagePos.AnchorPointY};

    std::string playTimeNumImg = manager->GetSkinConfigProp(SkinGroup::Playing, "Key#" + std::to_string(m_keyCount), "PlayTimeNum");
    if (playTimeNumImg.empty()) playTimeNumImg = manager->GetImageMapping(SkinGroup::Playing, "PlayTimeNum");
    if (playTimeNumImg.empty()) playTimeNumImg = "PlayTimeNum";

    std::vector<std::filesystem::path> numTimerPaths = {};
    for (int i = 0; i < 10; i++) {
      numTimerPaths.emplace_back(playingPath /
                                 (playTimeNumImg + std::to_string(i) + ".png"));

      if (!CheckSkinComponent(numTimerPaths.back())) {
        throw std::runtime_error(
            "Failed to load Timer Images 0-9, please check your skin folder.");
      }
    }

    std::string targetBarImg = manager->GetSkinConfigProp(SkinGroup::Playing, "Key#" + std::to_string(m_keyCount), "TargetBar");
    if (targetBarImg.empty()) targetBarImg = manager->GetImageMapping(SkinGroup::Playing, "TargetBar");
    if (targetBarImg.empty()) targetBarImg = "TargetBar";

    auto targetPos = manager->GetSprite(
        SkinGroup::Playing, "TargetBar"); // conf.GetSprite("TargetBar");
    std::vector<std::filesystem::path> targetBarPaths = {};
    if (std::filesystem::exists(playingPath / (targetBarImg + "-0.png"))) {
        int frame = 0;
        while (std::filesystem::exists(playingPath / (targetBarImg + "-" + std::to_string(frame) + ".png"))) {
            targetBarPaths.emplace_back(playingPath / (targetBarImg + "-" + std::to_string(frame) + ".png"));
            frame++;
        }
    } else if (std::filesystem::exists(playingPath / (targetBarImg + "0.png"))) {
        int frame = 0;
        while (std::filesystem::exists(playingPath / (targetBarImg + std::to_string(frame) + ".png"))) {
            targetBarPaths.emplace_back(playingPath / (targetBarImg + std::to_string(frame) + ".png"));
            frame++;
        }
    } else if (std::filesystem::exists(playingPath / (targetBarImg + ".png"))) {
        targetBarPaths.emplace_back(playingPath / (targetBarImg + ".png"));
    } else {
        throw std::runtime_error("Failed to load TargetBar Images, please check your skin folder.");
    }

    std::string playfooterImg = manager->GetSkinConfigProp(SkinGroup::Playing, "Key#" + std::to_string(m_keyCount), "Playfooter");
    if (playfooterImg.empty()) playfooterImg = manager->GetImageMapping(SkinGroup::Playing, "Playfooter");
    if (playfooterImg.empty()) playfooterImg = "PlayfieldFooter";

    try {
        auto playfooterPos = manager->GetPosition(SkinGroup::Playing, "Playfooter").front();
        if (std::filesystem::exists(playingPath / (playfooterImg + ".png"))) {
            m_PlayFooter = std::make_unique<Texture2D>(playingPath / (playfooterImg + ".png"));
            m_PlayFooter->Position = UDim2::fromOffset(playfooterPos.X, playfooterPos.Y);
            m_PlayFooter->AnchorPoint = {playfooterPos.AnchorPointX, playfooterPos.AnchorPointY};
        }
    } catch (...) {
        // PlayfieldFooter is optional, ignore if missing
    }

    m_targetBar = std::make_unique<Sprite2D>(targetBarPaths);
    m_targetBar->Position = UDim2::fromOffset(targetPos.X, targetPos.Y);
    m_targetBar->AnchorPoint = {targetPos.AnchorPointX, targetPos.AnchorPointY};
    m_targetBar->SetFPS(0.0);

    m_minuteNum = std::make_unique<NumericTexture>(numTimerPaths);
    auto minutePos = manager->GetNumeric(
        SkinGroup::Playing, "Minute"); // conf.GetNumeric("Minute");
    m_minuteNum->NumberPosition = IntToPos(minutePos[0].Direction);
    m_minuteNum->MaxDigits = minutePos[0].MaxDigit;
    m_minuteNum->FillWithZeros = false;
    m_minuteNum->Position = UDim2::fromOffset(minutePos[0].X, minutePos[0].Y);

    m_secondNum = std::make_unique<NumericTexture>(numTimerPaths);
    auto secondPos = manager->GetNumeric(
        SkinGroup::Playing, "Second"); // conf.GetNumeric("Second");
    m_secondNum->NumberPosition = IntToPos(secondPos[0].Direction);
    m_secondNum->MaxDigits = secondPos[0].MaxDigit;
    m_secondNum->FillWithZeros = true;
    m_secondNum->Position = UDim2::fromOffset(secondPos[0].X, secondPos[0].Y);

    auto pillsPosition = manager->GetPosition(
        SkinGroup::Playing, "Pill"); // conf.GetPosition("Pill");
    if (pillsPosition.size() < 5) {
      throw std::runtime_error(
          "Playing.ini : Positions : Pill : Not enough positions! (count < 5)");
    }

    for (int i = 0; i < 5; i++) {
      auto file = playingPath / ("Pill" + std::to_string(i) + ".png");

      auto pos = pillsPosition[i];
      m_pills[i] = std::move(std::make_unique<Texture2D>(file));
      m_pills[i]->Position = UDim2::fromOffset(pos.X, pos.Y);
      m_pills[i]->AnchorPoint = {pos.AnchorPointX, pos.AnchorPointY};
    }

    Chart *chart = (Chart *)EnvironmentSetup::GetObj("SONG");
    if (chart == nullptr) {
      throw std::runtime_error("Fatal error: Chart is null");
    }

    std::filesystem::path beatmapFile = EnvironmentSetup::GetPath("FILE");
    if (!beatmapFile.empty() && std::filesystem::exists(beatmapFile)) {
        // Storyboard parsing was here, now removed
    }

    m_videoPlayer.reset();

    int loadVideo = EnvironmentSetup::GetInt("LoadVideo");

    // Attempt to find and load video
    if (loadVideo == 1 && !chart->m_videoFile.empty()) {
        std::filesystem::path videoPath = chart->m_beatmapDirectory / chart->m_videoFile;
        Logs::Puts("[GameplayScene] Found video in chart: %s", chart->m_videoFile.c_str());
        Logs::Puts("[GameplayScene] Full path: %s", videoPath.string().c_str());
        if (std::filesystem::exists(videoPath)) {
            Logs::Puts("[GameplayScene] Video file exists, loading...");
            m_videoPlayer.reset(new VideoPlayer());
            auto pathU8 = videoPath.u8string();
            std::string u8PathStr(pathU8.begin(), pathU8.end());
            if (m_videoPlayer->Load(u8PathStr)) {
                Logs::Puts("[GameplayScene] Video loaded successfully and started playing. Offset: %f", chart->m_videoOffset);
                m_videoPlayer->Play();
            } else {
                Logs::Puts("[GameplayScene] Failed to load video via FFmpeg.");
                m_videoPlayer.reset();
            }
        } else {
            Logs::Puts("[GameplayScene] Video file does NOT exist on disk!");
        }
    } else {
        Logs::Puts("[GameplayScene] Chart m_videoFile is EMPTY!");
    }

    m_game.reset(new RhythmEngine());

    if (!m_game) {
      throw std::runtime_error("Failed to load game!");
    }

    m_game->SetHitPosition(HitPos);
    m_game->SetLaneOffset(useManiaLayout ? colStart : LaneOffset);
    if (useManiaLayout) m_game->SetColumnWidths(colWidths);
    if (!noteHeightStr.empty()) m_game->SetNoteHeight(std::stoi(noteHeightStr));

    int idx = 2;
    try {
      idx = std::stoi(Configuration::Load("Game", "GuideLine"));
    } catch (const std::invalid_argument &) {
      idx = 2;
    }

    m_game->SetGuideLineIndex(idx);

    auto OnTrackEvent = [&](GameTrackEvent e) {
      if (e.IsKeyEvent) {
        m_keyState[e.Lane] = e.State;
      } else {
        if (e.IsHitEvent) {
          if (e.IsHitLongEvent) {
            m_holdEffect[e.Lane]->Reset();
            m_drawHit[e.Lane] = false;
            m_drawHold[e.Lane] = e.State;

            if (!e.State) {
              m_hitEffect[e.Lane]->Reset();
              m_drawHit[e.Lane] = true;
            }
          } else {
            m_hitEffect[e.Lane]->Reset();
            m_drawHit[e.Lane] = true;
            m_drawHold[e.Lane] = false;
          }
        }
      }
    };

    m_game->ListenKeyEvent(OnTrackEvent);

    m_game->Load(chart);

    Keys keys[7] = {};
    std::string keyName = "Lane";
    if (chart->m_keyCount != 7) {
      keyName = std::to_string(chart->m_keyCount) + "_" + keyName;
    }

    for (int i = 0; i < chart->m_keyCount; i++) {
      auto value =
          Configuration::Load("KeyMapping", keyName + std::to_string(i + 1));
      auto key = static_cast<Keys>(SDL_GetScancodeFromName(value.c_str()));
      if (key == Keys::INVALID_KEY) {
        throw std::runtime_error(
            ("Unknown key: " + value + ", try check your keybind again!"));
      }

      keys[i] = key;
    }

    m_game->SetKeys(keys);

    auto hitEffectPos = manager->GetSprite(SkinGroup::Playing, "HitEffect");
    auto holdEffectPos = manager->GetSprite(SkinGroup::Playing, "HoldEffect");

    std::string hitEffectImg = manager->GetSkinConfigProp(SkinGroup::Playing, "Key#" + std::to_string(m_keyCount), "HitEffect");
    if (hitEffectImg.empty()) hitEffectImg = manager->GetImageMapping(SkinGroup::Playing, "HitEffect");
    if (hitEffectImg.empty()) hitEffectImg = "HitEffect";

    std::string holdEffectImg = manager->GetSkinConfigProp(SkinGroup::Playing, "Key#" + std::to_string(m_keyCount), "HoldEffect");
    if (holdEffectImg.empty()) holdEffectImg = manager->GetImageMapping(SkinGroup::Playing, "HoldEffect");
    if (holdEffectImg.empty()) holdEffectImg = "HoldEffect";

    std::vector<std::filesystem::path> HitEffect = {};
    if (std::filesystem::exists(arenaPath / (hitEffectImg + "-0.png"))) {
        int frame = 0;
        while (std::filesystem::exists(arenaPath / (hitEffectImg + "-" + std::to_string(frame) + ".png"))) {
            HitEffect.emplace_back(arenaPath / (hitEffectImg + "-" + std::to_string(frame) + ".png"));
            frame++;
        }
    } else if (std::filesystem::exists(arenaPath / (hitEffectImg + "0.png"))) {
        int frame = 0;
        while (std::filesystem::exists(arenaPath / (hitEffectImg + std::to_string(frame) + ".png"))) {
            HitEffect.emplace_back(arenaPath / (hitEffectImg + std::to_string(frame) + ".png"));
            frame++;
        }
    } else if (std::filesystem::exists(arenaPath / (hitEffectImg + ".png"))) {
        HitEffect.emplace_back(arenaPath / (hitEffectImg + ".png"));
    }

    std::vector<std::filesystem::path> holdEffect = {};
    if (std::filesystem::exists(arenaPath / (holdEffectImg + "-0.png"))) {
        int frame = 0;
        while (std::filesystem::exists(arenaPath / (holdEffectImg + "-" + std::to_string(frame) + ".png"))) {
            holdEffect.emplace_back(arenaPath / (holdEffectImg + "-" + std::to_string(frame) + ".png"));
            frame++;
        }
    } else if (std::filesystem::exists(arenaPath / (holdEffectImg + "0.png"))) {
        int frame = 0;
        while (std::filesystem::exists(arenaPath / (holdEffectImg + std::to_string(frame) + ".png"))) {
            holdEffect.emplace_back(arenaPath / (holdEffectImg + std::to_string(frame) + ".png"));
            frame++;
        }
    } else if (std::filesystem::exists(arenaPath / (holdEffectImg + ".png"))) {
        holdEffect.emplace_back(arenaPath / (holdEffectImg + ".png"));
    }

    auto lanePos = m_game->GetLanePos();
    auto laneSize = m_game->GetLaneSizes();

    if (m_autoImage) {
        float totalWidth = 0;
        for (int i = 0; i < m_keyCount; i++) {
            totalWidth += laneSize[i];
        }
        float centerX = lanePos[0] + (totalWidth / 2.0f);
        m_autoImage->Position = UDim2::fromOffset(centerX, 20);
        m_autoImage->AnchorPoint = {0.5, 0};
    }
    for (int i = 0; i < 7; i++) {
      m_hitEffect[i] = std::make_unique<Sprite2D>(
          HitEffect, hitEffectPos.FrameTime);
      m_holdEffect[i] = std::make_unique<Sprite2D>(
          holdEffect, holdEffectPos.FrameTime);
          
      if (i < imageFlip.size() && imageFlip[i]) {
          m_hitEffect[i]->FlipX = true;
          m_holdEffect[i]->FlipX = true;
      }

      m_hitEffect[i]->SetFPS(hitEffectPos.FrameTime);
      m_holdEffect[i]->SetFPS(holdEffectPos.FrameTime);
      m_hitEffect[i]->SetIndex(m_hitEffect[i]->GetFrameCount() - 1);
      m_hitEffect[i]->AlphaBlend = true;
      m_holdEffect[i]->AlphaBlend = true;

      float pos = std::ceil(lanePos[i] + (laneSize[i] / 2.0f));
      auto hitPos = UDim2::fromOffset(pos, HitPos - 15) +
                    UDim2::fromOffset(hitEffectPos.X, hitEffectPos.Y);
      auto holdPos = UDim2::fromOffset(pos, HitPos - 15) +
                     UDim2::fromOffset(holdEffectPos.X, holdEffectPos.Y);

      m_hitEffect[i]->Position = hitPos;
      m_holdEffect[i]->Position = holdPos;
      m_hitEffect[i]->AnchorPoint = {hitEffectPos.AnchorPointX,
                                     hitEffectPos.AnchorPointY};
      m_holdEffect[i]->AnchorPoint = {holdEffectPos.AnchorPointX,
                                      holdEffectPos.AnchorPointY};

      if (conKeyLight.size() < m_keyCount && m_keyLighting[i] && m_keyLighting[i]->GetTexture()) {
          int origWidth = m_keyLighting[i]->GetTexture()->GetOriginalRECT().right;
          int origHeight = m_keyLighting[i]->GetTexture()->GetOriginalRECT().bottom;
          
          int displayWidth = origWidth;
          if (autoWidth) {
              displayWidth = laneSize[i];
          }
          m_keyLighting[i]->Size = UDim2::fromOffset(displayWidth, origHeight);
          
          char align = 'C';
          if (i < imageAlign.size()) align = imageAlign[i];

          if (align == 'L') {
              m_keyLighting[i]->Position = UDim2::fromOffset(lanePos[i], HitPos);
          } else if (align == 'R') {
              m_keyLighting[i]->Position = UDim2::fromOffset(lanePos[i] + laneSize[i] - displayWidth, HitPos);
          } else { // Center
              m_keyLighting[i]->Position = UDim2::fromOffset(lanePos[i] + (laneSize[i] - displayWidth) / 2.0f, HitPos);
          }
          m_keyLighting[i]->AnchorPoint = {0.0f, 1.0f};
      }
    }

    bool IsHD = EnvironmentSetup::GetInt("Hidden") == 1;
    bool IsFL = EnvironmentSetup::GetInt("Flashlight") == 1;
    bool IsSD = EnvironmentSetup::GetInt("Sudden") == 1;
    bool IsRD = EnvironmentSetup::GetInt("Random") == 1;
    bool IsMR = EnvironmentSetup::GetInt("Mirror") == 1;
    bool IsPC = EnvironmentSetup::GetInt("Panic") == 1;

    if (IsHD || IsFL || IsSD) {
      std::vector<Segment> segments;

      if (IsHD) {
        segments = {{0.00f, 0.45f, {0, 0, 0, 0}, {0, 0, 0, 0}},
                    {0.45f, 0.50f, {0, 0, 0, 0}, {0, 0, 0, 255}},
                    {0.50f, 1.00f, {0, 0, 0, 255}, {0, 0, 0, 255}}};
      }
      if (IsFL) {
        segments = {{0.00f, 0.33f, {0, 0, 0, 255}, {0, 0, 0, 255}},
                    {0.33f, 0.38f, {0, 0, 0, 255}, {0, 0, 0, 0}},
                    {0.38f, 0.61f, {0, 0, 0, 0}, {0, 0, 0, 0}},
                    {0.61f, 0.67f, {0, 0, 0, 0}, {0, 0, 0, 255}},
                    {0.67f, 1.00f, {0, 0, 0, 255}, {0, 0, 0, 255}}};
      }
      if (IsSD) {
        segments = {{0.00f, 0.50f, {0, 0, 0, 255}, {0, 0, 0, 255}},
                    {0.50f, 0.55f, {0, 0, 0, 255}, {0, 0, 0, 0}},
                    {0.55f, 1.00f, {0, 0, 0, 0}, {0, 0, 0, 0}}};
      }

      int imageWidth =
          static_cast<int>(std::accumulate(laneSize, laneSize + 7, 0.0f));
      int imageHeight = HitPos;
      int imagePos = static_cast<int>(lanePos[0]);

      std::vector<uint8_t> imageBuffer = ImageGenerator::GenerateGradientImage(
          imageWidth, imageHeight, segments);

      m_laneHideImage =
          std::make_unique<Texture2D>(imageBuffer.data(), imageBuffer.size());
      m_laneHideImage->Position = UDim2::fromOffset(imagePos, 0);
      m_laneHideImage->Size = UDim2::fromOffset(imageWidth, imageHeight);
    }

    bool VisualModEnabled =
        IsHD || IsFL || IsSD; // Check if any of the VisualMods are active (Hidden or Flashlight)
    bool NoteModEnabled =
        IsMR || IsRD ||
        IsPC; // Check if any of the NoteMods are active (Mirror or Random)

    if (VisualModEnabled) { // Draw VisualMods (Hidden, Flashlight, and Sudden)
      if (IsHD) {
        std::string VisualModImage = "ModHidden.png";
        auto VisualModfilename = playingPath / VisualModImage;
        if (!std::filesystem::exists(VisualModfilename)) {
            VisualModfilename = std::filesystem::current_path() / "Resources" / "Mods" / VisualModImage;
        }

        try {
          auto visualModPos =
              manager->GetPosition(SkinGroup::Playing, "VisualMods").front();
          m_visualMod = std::make_unique<Texture2D>(VisualModfilename);
          m_visualMod->Position =
              UDim2::fromOffset(visualModPos.X, visualModPos.Y);
          m_visualMod->AnchorPoint = {visualModPos.AnchorPointX,
                                      visualModPos.AnchorPointY};
        } catch (std::runtime_error &) {
        }
      }

      if (IsFL) {
        std::string VisualModImage = "ModFlashlight.png";
        auto VisualModfilename = playingPath / VisualModImage;
        if (!std::filesystem::exists(VisualModfilename)) {
            VisualModfilename = std::filesystem::current_path() / "Resources" / "Mods" / VisualModImage;
        }

        try {
          auto visualModPos =
              manager->GetPosition(SkinGroup::Playing, "VisualMods").front();
          m_visualMod = std::make_unique<Texture2D>(VisualModfilename);
          m_visualMod->Position =
              UDim2::fromOffset(visualModPos.X, visualModPos.Y);
          m_visualMod->AnchorPoint = {visualModPos.AnchorPointX,
                                      visualModPos.AnchorPointY};
        } catch (std::runtime_error &) {
        }
      }
      if (IsSD) {
        std::string VisualModImage = "ModSudden.png";
        auto VisualModfilename = playingPath / VisualModImage;
        if (!std::filesystem::exists(VisualModfilename)) {
            VisualModfilename = std::filesystem::current_path() / "Resources" / "Mods" / VisualModImage;
        }

        try {
          auto visualModPos =
              manager->GetPosition(SkinGroup::Playing, "VisualMods").front();
          m_visualMod = std::make_unique<Texture2D>(VisualModfilename);
          m_visualMod->Position =
              UDim2::fromOffset(visualModPos.X, visualModPos.Y);
          m_visualMod->AnchorPoint = {visualModPos.AnchorPointX,
                                      visualModPos.AnchorPointY};
        } catch (std::runtime_error &) {
        }
      }
    }

    if (NoteModEnabled) { // Draw NoteMods (Mirror, Random, and Panic)
      if (IsMR) {
        std::string NoteModImage = "ModMirror.png";
        auto NoteModfilename = playingPath / NoteModImage;
        if (!std::filesystem::exists(NoteModfilename)) {
            NoteModfilename = std::filesystem::current_path() / "Resources" / "Mods" / NoteModImage;
        }

        try {
          auto noteModPos =
              manager->GetPosition(SkinGroup::Playing, "NoteMods").front();
          m_noteMod = std::make_unique<Texture2D>(NoteModfilename);
          m_noteMod->Position = UDim2::fromOffset(noteModPos.X, noteModPos.Y);
          m_noteMod->AnchorPoint = {noteModPos.AnchorPointX,
                                    noteModPos.AnchorPointY};
        } catch (std::runtime_error &) {
        }
      }

      if (IsRD) {
        std::string NoteModImage = "ModRandom.png";
        auto NoteModfilename = playingPath / NoteModImage;
        if (!std::filesystem::exists(NoteModfilename)) {
            NoteModfilename = std::filesystem::current_path() / "Resources" / "Mods" / NoteModImage;
        }

        try {
          auto noteModPos =
              manager->GetPosition(SkinGroup::Playing, "NoteMods").front();
          m_noteMod = std::make_unique<Texture2D>(NoteModfilename);
          m_noteMod->Position = UDim2::fromOffset(noteModPos.X, noteModPos.Y);
          m_noteMod->AnchorPoint = {noteModPos.AnchorPointX,
                                    noteModPos.AnchorPointY};
        } catch (std::runtime_error &) {
        }
      }
      if (IsPC) {
        std::string NoteModImage = "ModPanic.png";
        auto NoteModfilename = playingPath / NoteModImage;
        if (!std::filesystem::exists(NoteModfilename)) {
            NoteModfilename = std::filesystem::current_path() / "Resources" / "Mods" / NoteModImage;
        }

        try {
          auto noteModPos =
              manager->GetPosition(SkinGroup::Playing, "NoteMods").front();
          m_noteMod = std::make_unique<Texture2D>(NoteModfilename);
          m_noteMod->Position = UDim2::fromOffset(noteModPos.X, noteModPos.Y);
          m_noteMod->AnchorPoint = {noteModPos.AnchorPointX,
                                    noteModPos.AnchorPointY};
        } catch (std::runtime_error &) {
        }
      }
    }

    std::string modSelected;

    if (IsHD)
      modSelected += "Hidden, ";
    if (IsFL)
      modSelected += "Flashlight, ";
    if (IsSD)
      modSelected += "Sudden, ";
    if (IsRD)
      modSelected += "Random, ";
    if (IsMR)
      modSelected += "Mirror, ";
    if (IsPC)
      modSelected += "Panic, ";

    if (!modSelected.empty()) {
      modSelected.erase(modSelected.size() - 2);
      Logs::Puts(("[Gameplay] Using modifier " + modSelected).c_str());
    } else {
      Logs::Puts("[Gameplay] Not using any modifier");
    }

    int difficulty = EnvironmentSetup::GetInt("Difficulty"); // Debug
    switch (difficulty) {
    case 0:
      Logs::Puts("[ScoreManager] Using difficulty Easy");
      break;
    case 1:
      Logs::Puts("[ScoreManager] Using difficulty Normal");
      break;
    case 2:
      Logs::Puts("[ScoreManager] Using difficulty Hard");
      break;
    default:
      Logs::Puts("[ScoreManager] Unknown difficulty");
      break;
    }

    auto OnHitEvent = [&](NoteHitInfo info) {
      m_scoreTimer = 0;
      m_judgeTimer = 0;
      m_judgeSize = 0.6;

      m_drawCombo = true;
      m_drawJudge = true;

      m_comboTimer = 0;
      m_comboLogo->Reset();
      m_judgeIndex = (int)info.Result;

      if (m_judgementSprite.find(m_judgeIndex) != m_judgementSprite.end() && m_judgementSprite[m_judgeIndex] != nullptr) {
          m_judgementSprite[m_judgeIndex]->Reset();
      }
    };

    m_game->GetScoreManager()->ListenHit(OnHitEvent);

    auto OnJamEvent = [&](int combo) {
      m_drawJam = true;
      m_jamTimer = 0;
      m_jamLogo->Reset();
    };

    m_game->GetScoreManager()->ListenJam(OnJamEvent);

    auto OnLongComboEvent = [&] {
      m_lnTimer = 0;
      m_drawLN = true;
      m_lnLogo->Reset();
    };

    m_game->GetScoreManager()->ListenLongNote(OnLongComboEvent);

  }

  catch (SDLException &e) {
    MsgBox::Show("GameplayError", "Image Error", e.what(), MsgBoxType::OK);
    m_resourceFucked = true;
    return true;
  } catch (std::exception &e) {
    MsgBox::Show("GameplayError", "Error", e.what(), MsgBoxType::OK);
    m_resourceFucked = true;
    return true;
  }

  SceneManager::GetInstance()->SetFrameLimitMode(FrameLimitMode::GAME);
  m_starting = false;

  return true;
}

bool GameplayScene::Detach() {
  for (int i = 0; i < 7; i++) {
    m_keyLighting[i].reset();
    m_keyButtons[i].reset();
    m_hitEffect[i].reset();
    m_holdEffect[i].reset();
  }

  for (int i = 0; i < 5; i++) {
    m_pills[i].reset();
  }

  for (int i = 0; i < 4; i++) {
    m_judgement[i].reset();
    m_judgementSprite[i].reset();
  }

  m_PlayBG.reset();
  m_Playfield.reset();
  m_PlayFooter.reset();
  m_exitBtn.reset();

  m_noteMod.reset();
  m_visualMod.reset();

  m_jamGauge.reset();
  m_waveGage.reset();
  m_jamLogo.reset();
  m_lifeBar.reset();
  m_lnLogo.reset();
  m_comboLogo.reset();
  m_targetBar.reset();
  m_laneHideImage.reset();

  m_lnComboNum.reset();
  m_jamNum.reset();
  m_scoreNum.reset();
  m_comboNum.reset();
  m_minuteNum.reset();
  m_secondNum.reset();

  if (m_game) {
    auto manager = m_game->GetScoreManager();
    if (manager) {
      auto score = manager->GetScore();

      EnvironmentSetup::SetInt("Score", std::get<0>(score));
      EnvironmentSetup::SetInt("Cool", std::get<1>(score));
      EnvironmentSetup::SetInt("Good", std::get<2>(score));
      EnvironmentSetup::SetInt("Bad", std::get<3>(score));
      EnvironmentSetup::SetInt("Miss", std::get<4>(score));
      EnvironmentSetup::SetInt("JamCombo", std::get<5>(score));
      EnvironmentSetup::SetInt("MaxJamCombo", std::get<6>(score));
      EnvironmentSetup::SetInt("Combo", std::get<7>(score));
      EnvironmentSetup::SetInt("MaxCombo", std::get<8>(score));
      EnvironmentSetup::SetInt("LNCombo", std::get<9>(score));
      EnvironmentSetup::SetInt("LNMaxCombo", std::get<10>(score));
    }
  }

  m_game.reset();
  m_title.reset();
  m_exitButtonFunc.reset();

  auto songBG = (Texture2D *)EnvironmentSetup::GetObj("SongBackground");
  if (songBG) {
    songBG->TintColor = Color3::FromRGB(255, 255, 255);
  }

  SceneManager::GetInstance()->SetFrameLimitMode(FrameLimitMode::MENU);
  return true;
}

void *GameplayScene::CreateScreenshotWin32() { return nullptr; }
