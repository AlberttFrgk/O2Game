#include "SongSelectScene.h"
#include <array>
#include <iostream>

#include "Audio/AudioManager.h"
#include "Configuration.h"
#include "Inputs/Keys.h"
#include "MsgBox.h"
#include "Rendering/Window.h"
#include "SceneManager.h"
#include "Texture/MathUtils.h"
#include <sstream>

#include "Imgui/ImguiUtil.h"
#include "Imgui/imgui.h"
#include "Imgui/imgui_extends.h"
#include "Imgui/imgui_internal.h"

#include "Exception/SDLException.h"
#include "Fonts/FontResources.h"

#include "../Engine/SkinConfig.hpp"
#include "../Engine/SkinManager.hpp"
#include "../EnvironmentSetup.hpp"
#include "../GameScenes.h"
#include "../Resources/GameDatabase.h"
#include "../Resources/MusicListMaker.h"

#include "../Data/BMS.hpp"
#include "../Data/Chart.hpp"
#include "../Data/Util/Util.hpp"
#include "../Data/osu.hpp"

static std::wstring OpenFilePrompt() {
  OPENFILENAMEW ofn;
  wchar_t szFile[MAX_PATH] = {0};

  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = NULL;
  ofn.lpstrFilter =
      L"All Supported Files "
      L"(*.ojn;*.osu;*.bms;*.bme;*.bml;*)\0*.ojn;*.osu;*.bms;*.bme;*.bml\0";
  ofn.lpstrFile = szFile;
  ofn.nMaxFile = MAX_PATH;
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
  if (GetOpenFileNameW(&ofn)) {
    return szFile;
  } else {
    return L"";
  }
}

static std::array<std::string, 8> Mods = {"Mirror",     "Random",   "Panic",
                                          "Rearrange",  "Autoplay", "Hidden",
                                          "Flashlight", "Sudden"};

SongSelectScene::SongSelectScene() {
  index = -1;
  memset(lanePos, 0, sizeof(lanePos));
  m_externalItem = {};

  int *data = new int[7];
  EnvironmentSetup::SetObj("LaneData", data);
}

SongSelectScene::~SongSelectScene() {
  if (m_bgm) {
    m_bgm.reset();
  }

  m_songBackground.reset();
  m_background.reset();

  delete[] EnvironmentSetup::GetObj("LaneData");
  EnvironmentSetup::SetObj("LaneData", nullptr);
}

void SongSelectScene::Render(double delta) {
  ImguiUtil::NewFrame();

  auto window = GameWindow::GetInstance();

  bPlay = false;
  bOpenFile = false;
  bOptionPopup = false;
  bSelectNewSong = false;
  bOpenSongContext = false;
  bOpenEditor = false;
  bOpenRearrange = false;
  bQuit = false;
  bScaleOutput = window->IsScaleOutput();

  auto windowNextSz =
      ImVec2((float)window->GetBufferWidth(), (float)window->GetBufferHeight());
  if (m_songBackground) {
    m_songBackground->Draw();
  } else {
    if (m_background) {
      m_background->Draw();
    }
  }

  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(MathUtil::ScaleVec2(windowNextSz));

  ImGui::GetStyle().DisabledAlpha =
      std::clamp(currentAlpha / 100.0f, 0.00000001f, 1.0f);
  auto flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
               ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
               ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_MenuBar |
               ImGuiWindowFlags_NoBringToFrontOnFocus;

  ImGui::BeginDisabled(is_departing);

  if (ImGui::Begin("#SongSelectMenuBar", nullptr, flags)) {
    if (ImGui::BeginMenuBar()) {

      if (ImGui::Button("Quit", MathUtil::ScaleVec2(ImVec2(50, 0)))) {
        bQuit = true;
      }

      if (EnvironmentSetup::GetInt("FileOpen") == 1) {
        if (ImGui::Button("Cancel File", MathUtil::ScaleVec2(ImVec2(50, 0)))) {
          EnvironmentSetup::SetInt("FileOpen", 0);
          EnvironmentSetup::SetPath("FILE", "");

          Audio *extBgm = AudioManager::GetInstance()->Get("ExternalBGM");
          if (extBgm)
            extBgm->FadeOut();

          if (index != -1) {
            DB_MusicItem item = GameDatabase::GetInstance()->Find(index);
            if (item.Id != -1 && item.CoverSize > 0) {
              std::filesystem::path file;
              if (item.FilePath[0] != '\0' && std::filesystem::exists(item.FilePath)) {
                file = item.FilePath;
              } else {
                file = GameDatabase::GetInstance()->GetPath() / ("o2ma" + std::to_string(item.Id) + ".ojn");
              }
              try {
                auto buffer = O2::OJN::LoadOJNFile(file);
                buffer.seekg(item.CoverOffset);
                std::vector<uint8_t> coverData(item.CoverSize);
                buffer.read((char *)coverData.data(), item.CoverSize);
                m_songBackground = std::make_unique<Texture2D>(coverData.data(),
                                                               item.CoverSize);
                m_songBackground->Size = UDim2::fromOffset(
                    GameWindow::GetInstance()->GetBufferWidth(),
                    GameWindow::GetInstance()->GetBufferHeight());
              } catch (...) {
                m_songBackground.reset();
              }
            } else {
              m_songBackground.reset();
            }
          } else {
            m_songBackground.reset();
          }

          Audio *bgm = AudioManager::GetInstance()->Get("BGM");
          if (bgm)
            bgm->FadeIn();

          if (m_bgm && m_bgm->IsPlaying())
            m_bgm->Stop();
        }
      } else {
        if (ImGui::Button("Open File", MathUtil::ScaleVec2(ImVec2(50, 0)))) {
          bOpenFile = true;
        }
      }

      if (ImGui::Button("Settings", MathUtil::ScaleVec2(ImVec2(50, 0)))) {
        bOptionPopup = true;
      }

      ImGuiIO &io = ImGui::GetIO();

      if (EnvironmentSetup::GetInt("FileOpen") == 1) {
        std::filesystem::path p = EnvironmentSetup::GetPath("FILE");
        ImGui::Text("Selected song: %s", p.filename().string().c_str());
      } else {
        ImGui::Text("Select Song");
      }

      std::string text = "Game engines by Estrol, Effects by Albert";
      auto textWidth = ImGui::CalcTextSize(text.c_str()).x;

      ImGui::SameLine(MathUtil::ScaleVec2(ImVec2(windowNextSz.x, 0)).x -
                      textWidth - 15);
      ImGui::Text(text.c_str());

      ImGui::EndMenuBar();
    }

    switch (scene_index) {
    case 0: {
      OnGameSelectMusic(delta);
      break;
    }
    case 1: {
      OnGameLoadMusic(delta);
      break;
    }
    }

    // fuck it, whatever rebuild the font, this thing is required!
    if (ImGui::GetCurrentWindow()->Flags & ImGuiWindowFlags_ChildWindow) {
      ImGui::EndChild();
    }

    ImGui::End();
  }

  if (ImGui::IsKeyPressed(ImGuiKey_F5, false)) {
    MsgBox::Show("DialogResetList", "Notice", "Do you want refresh map list?",
                 MsgBoxType::YESNO);
  }

  if (ImGui::IsKeyPressed(ImGuiKey_F3, false)) {
    bPlay = true;
  }

  if (bOptionPopup) {
    SceneManager::OverlayShow(GameOverlay::SETTINGS);
  }

  if (bPlay && index == -1 && EnvironmentSetup::GetInt("FileOpen") == 0) {
    MsgBox::Show("MustSelectSong", "Error", "You must select a song to play!",
                 MsgBoxType::OK);
  }

  if (bOpenSongContext) {
    ImGui::OpenPopup("###pop_up_right_click_song_context");
  }

  if (bOpenRearrange) {
    memset(lanePos, NULL, sizeof(lanePos));
    ImGui::OpenPopup("###open_rearrange");
  }

  ImGuiIO &io = ImGui::GetIO();

  ImGui::SetNextWindowPos(
      ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
      ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  if (ImGui::BeginPopupModal("Set lane position###open_rearrange", NULL,
                             ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_AlwaysAutoResize)) {
    int state = EnvironmentSetup::GetInt("rearrange_dialog_state");
    switch (state) {
    case 1: {
      ImGui::Text("%s", EnvironmentSetup::Get("rearrange_dialog_msg").c_str());

      if (ImGui::Button("OK", MathUtil::ScaleVec2(50, 0))) {
        EnvironmentSetup::SetInt("rearrange_dialog_state", 0);
      }
      break;
    }

    default: {
      int *data = reinterpret_cast<int *>(EnvironmentSetup::GetObj("LaneData"));
      ImGui::InputText("###LanePos", lanePos, sizeof(lanePos),
                       ImGuiInputTextFlags_CharsDecimal);

      ImGui::NewLine();
      if (ImGui::Button("OK", MathUtil::ScaleVec2(50, 0))) {
        bool error = false;

        int laneIndex[7] = {};
        memset(laneIndex, -1, sizeof(laneIndex));

        for (int i = 0; i < 7; i++) {
          char c = lanePos[i];

          try {
            // check if c is a number
            if (!std::isdigit(c)) {
              throw std::invalid_argument(
                  "Invalid input found!, Please input correct number "
                  "between 0 to 6\n\nExample: 0123456 or 6543210 or "
                  "6540123");
            }

            // parse char to number
            int num = std::stoi(std::string(1, c));

            // check range between 0 to 6
            if (num < 0 || num > 6) {
              throw std::invalid_argument(
                  "Invalid input found!, Please input correct number "
                  "between 0 to 6\n\nExample: 0123456 or 6543210 or "
                  "6540123");
            }

            // find duplicates
            for (int j = 0; j < 7; j++) {
              if (j == i) {
                continue;
              }

              if (num == laneIndex[j]) {
                throw std::invalid_argument(
                    "Duplicated lane position found!, Please input "
                    "different lane from 0 to 6\n\nExample: 0123456 or "
                    "6543210 or 6540123");
              }
            }

            laneIndex[i] = num;
          } catch (std::invalid_argument e) {
            error = true;

            EnvironmentSetup::Set("rearrange_dialog_msg", e.what());
            EnvironmentSetup::SetInt("rearrange_dialog_state", 1);
          }
        }

        if (!error) {
          memcpy(data, laneIndex, sizeof(laneIndex));

          ImGui::CloseCurrentPopup();
        }
      }

      ImGui::SameLine();

      if (ImGui::Button("Cancel", MathUtil::ScaleVec2(50, 0))) {
        EnvironmentSetup::SetInt("Rearrange", 0);
        ImGui::CloseCurrentPopup();
      }

      break;
    }
    }

    ImGui::EndPopup();
  }

  bool changeResolution = false;
  ImGui::SetNextWindowPos(
      ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
      ImGuiCond_Always, ImVec2(0.5f, 0.5f));

  if (ImGui::BeginPopupModal("###pop_up_right_click_song_context", NULL,
                             ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_AlwaysAutoResize)) {
    std::string text = "What do you want to do?";
    ImVec2 size = ImGui::CalcTextSize(text.c_str());

    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - size.x) * 0.5f);
    ImGui::Text("%s", text.c_str());

    ImGui::NewLine();

    if (ImGui::Button("Play", MathUtil::ScaleVec2(295, 25))) {
      bPlay = true;
      ImGui::CloseCurrentPopup();
    }

    ImGui::NewLine();
    if (ImGui::Button("Open in Editor", MathUtil::ScaleVec2(295, 25))) {
      bOpenEditor = true;
      ImGui::CloseCurrentPopup();
    }

    ImGui::NewLine();
    if (ImGui::Button("Exit this menu", MathUtil::ScaleVec2(295, 25)) ||
        ImGui::IsKeyDown(ImGuiKey_Escape)) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  ImGui::EndDisabled();

  EnvironmentSetup::SetInt("Key", index);

  if (!is_update_bgm &&
      (index != -1 || EnvironmentSetup::GetInt("FileOpen") == 1) && isWait) {
    waitTime += (float)delta;

    if (waitTime > 0.25f) {
      isWait = false;
      bSelectNewSong = true;
    }
  }

  if (bOpenFile) {
    std::wstring songfile = OpenFilePrompt();
    if (!songfile.empty()) {
      EnvironmentSetup::SetInt("FileOpen", 1);
      EnvironmentSetup::SetPath("FILE", songfile);
      bSelectNewSong = true;
    }
  }

  if ((index != -1 || EnvironmentSetup::GetInt("FileOpen") == 1) &&
      bSelectNewSong) {
    SaveConfiguration();

    DB_MusicItem curItem = {};
    if (index != -1) {
      curItem = GameDatabase::GetInstance()->Find(index);
    }

    if (curItem.Id != -1 && (curItem.Category == 1 || curItem.Category == 2)) {
      LoadDifficultyOptions(curItem);
    } else {
      m_difficultyOptions.clear();
      EnvironmentSetup::SetInt("FileOpen", 0);
      EnvironmentSetup::SetInt("Key", curItem.Id != -1 ? curItem.Id : index);
      if (curItem.FilePath[0] != '\0') {
        EnvironmentSetup::SetPath("FILE", curItem.FilePath);
      }
      LoadChartImage();

      if (m_bgm->IsPlaying()) {
        m_bgm->Stop();
      }

      Audio *extBgm = AudioManager::GetInstance()->Get("ExternalBGM");
      if (extBgm) {
        extBgm->FadeOut();
      }

      if (EnvironmentSetup::GetInt("FileOpen") == 0) {
        m_bgm->Load();
      } else {
        std::string ext = EnvironmentSetup::GetPath("FILE").extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext == ".ojn" || ext == ".bms" || ext == ".bme" || ext == ".bml" || ext == ".bmsc") {
          m_bgm->Load();
        } else {
          Audio *bgm = AudioManager::GetInstance()->Get("BGM");
          if (bgm) {
            bgm->SetVolume(100);
            bgm->FadeOut();
          }

          std::string currentFile = EnvironmentSetup::GetPath("FILE").string();
          SceneManager::ExecuteAfter(1000, [currentFile]() {
            if (EnvironmentSetup::GetInt("FileOpen") == 1 &&
                EnvironmentSetup::GetPath("FILE").string() == currentFile) {
              Audio *extBgm = AudioManager::GetInstance()->Get("ExternalBGM");
              if (extBgm) {
                extBgm->SetVolume(100);
                extBgm->Play();
              }
            }
          });
        }
      }
    }
  }

  if (bPlay && (index != -1 || EnvironmentSetup::GetInt("FileOpen") == 1) &&
      !is_departing) {
    is_departing = true;
    SaveConfiguration();

    // Force LoadingScene to load the external file
    if (EnvironmentSetup::GetInt("FileOpen") == 1) {
      EnvironmentSetup::SetInt("Key", -1);
      EnvironmentSetup::SetObj("SONG", nullptr);
    }

    EnvironmentSetup::SetObj("SongBackground", m_songBackground ? m_songBackground.get() : nullptr);

    nextAlpha = 0;
    SceneManager::ExecuteAfter(
        600, [this]() { SceneManager::ChangeScene(GameScene::LOADING); });
  }

  if (bOpenEditor) {
    is_departing = true;
    SaveConfiguration();

    EnvironmentSetup::SetObj("SongBackground", m_songBackground ? m_songBackground.get() : nullptr);

    SceneManager::DisplayFade(
        100, [this]() { SceneManager::ChangeScene(GameScene::EDITOR); });
  }

  if (bQuit) {
    SDL_Event e = {};
    e.type = SDL_QUIT;
    SDL_PushEvent(&e);
  }
}

void SongSelectScene::OnGameLoadMusic(double delta) {
  auto db = GameDatabase::GetInstance();
  auto path = db->GetPath();
  auto &io = ImGui::GetIO();

  m_lastTime += static_cast<float>(delta);

  bool bExit = false;
  if (m_tempMusicIndex < m_tempMusicLists.size()) {
    if (m_lastTime >= 0.0025) {
      m_lastTime = 0;

      file = m_tempMusicLists[m_tempMusicIndex];
      MusicListMaker::Insert(file);

      m_tempMusicIndex++;
    }
  } else {
    scene_index = 0;
    m_tempMusicLists.clear();
    m_musicList = db->FindAll(m_selectedCategory);
    if (!m_musicList.empty()) {
      index = m_musicList[0].Id;
      bSelectNewSong = true;
    }
  }

  if (!file.empty() && !MsgBox::Any()) {
    ImGui::OpenPopup("###imgui_ui_load_music");
  }

  ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize |
                           ImGuiWindowFlags_NoSavedSettings |
                           ImGuiWindowFlags_AlwaysAutoResize;
  ImGui::SetNextWindowPos(
      ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
      ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(MathUtil::ScaleVec2(250, 80));
  if (ImGui::BeginPopupModal("Rebuild database....###imgui_ui_load_music",
                             nullptr, flags)) {
    ImGui::NewLine();

    std::string text = "Processing file: " + file.filename().string();
    imgui_extends::TextAligment(text.c_str(), 0.5f);

    if (scene_index == 0) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::NewLine();
    ImGui::EndPopup();
  }
}

void SongSelectScene::OnGameSelectMusic(double delta) {
  auto music = GameDatabase::GetInstance();
  auto window = GameWindow::GetInstance();
  auto windowNextSz =
      ImVec2((float)window->GetBufferWidth(), (float)window->GetBufferHeight());
  currentDifficulty = EnvironmentSetup::GetInt("Difficulty");

  // create child window
  if (ImGui::BeginChild("#Container1", MathUtil::ScaleVec2(ImVec2(200, 500)))) {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 3));
    if (ImGui::BeginChild("#SongSelectDetailsChild",
                          MathUtil::ScaleVec2(ImVec2(200, 200)), true)) {
      ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
      ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0, 0));

      DB_MusicItem item = {};
      if (EnvironmentSetup::GetInt("FileOpen") == 1) {
        item = m_externalItem;
      } else if (index != -1) {
        auto it = std::find_if(
            m_musicList.begin(), m_musicList.end(),
            [&](const DB_MusicItem &item) { return item.Id == index; });

        if (it != m_musicList.end()) {
          item = *it;
        }
      }

      ImVec4 color = ImGui::GetStyleColorVec4(ImGuiCol_Button);
      float textBgWidth = ImGui::GetContentRegionAvail().x - 4.0f;

      ImGui::Text("Title\r");
      imgui_extends::TextBackground(color, ImVec2(textBgWidth, 0), "%s",
                                    (const char *)item.Title);

      ImGui::Text("Artist / Composer\r");
      imgui_extends::TextBackground(color, ImVec2(textBgWidth, 0), "%s",
                                    (const char *)item.Artist);

      ImGui::Text("Notecharter\r");
      imgui_extends::TextBackground(color, ImVec2(textBgWidth, 0), "%s",
                                    (const char *)item.Noter);

      ImGui::Text("Total Notes\r");

      difficulty = EnvironmentSetup::GetInt("Difficulty");
      int count = item.Id == -1 && EnvironmentSetup::GetInt("FileOpen") == 0
                      ? 0
                      : item.MaxNotes[difficulty];
      imgui_extends::TextBackground(color, ImVec2(textBgWidth, 0), "%d",
                                    count);

      ImGui::Text("BPM\r");
      float bpm = item.Id == -1 && EnvironmentSetup::GetInt("FileOpen") == 0
                      ? 0.0f
                      : item.BPM;
      imgui_extends::TextBackground(color, ImVec2(textBgWidth, 0), "%.2f",
                                    bpm);

      ImGui::PopItemFlag();
      ImGui::PopStyleVar();

      ImGui::EndChild();
    }

    if (ImGui::BeginChild("#test2",
                          ImVec2(MathUtil::ScaleVec2(ImVec2(200, 0)).x, 0),
                          true)) {
      ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 2));

      DB_MusicItem currentItem = {};
      if (index != -1) {
        auto it = std::find_if(m_musicList.begin(), m_musicList.end(),
                               [&](const DB_MusicItem &it) { return it.Id == index; });
        if (it != m_musicList.end()) {
          currentItem = *it;
        }
      }

      if (currentItem.Category == 0) {
        std::vector<std::string> difficulty = {"EZ", "NM", "HD"};

        ImGui::Text("Note difficulty");

        for (int i = 0; i < difficulty.size(); i++) {
          int diffIdx = currentDifficulty;

          if (diffIdx == i) {
            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha,
                                ImGui::GetStyle().Alpha * 0.5f);
          }

          if (ImGui::Button(difficulty[i].c_str(),
                            MathUtil::ScaleVec2(ImVec2(30, 30)))) {
            EnvironmentSetup::SetInt("Difficulty", i);
            currentDifficulty = i;
          }

          if (diffIdx == i) {
            ImGui::PopItemFlag();
            ImGui::PopStyleVar();
          }

          if (i != difficulty.size() - 1) {
            ImGui::SameLine();
          }
        }
      } else {
        if (currentItem.Category == 1) {
          ImGui::Text("osu!mania Difficulty");
        } else {
          ImGui::Text("BMS Difficulty");
        }

        if (ImGui::BeginChild("###DiffScrollList",
                              ImVec2(0, MathUtil::ScaleVec2(0, 85).y),
                              true, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
          if (m_difficultyOptions.empty()) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No charts");
          } else {
            for (int d = 0; d < static_cast<int>(m_difficultyOptions.size()); d++) {
              const auto &opt = m_difficultyOptions[d];
              bool isSelected = (d == m_selectedDiffIndex);

              char label[256];
              if (currentItem.Category == 1) {
                snprintf(label, sizeof(label), "[%dK] %s (OD %.1f)###Diff_%d",
                         opt.KeyCount, opt.Name.c_str(), opt.Rating, d);
              } else {
                snprintf(label, sizeof(label), "[%dK] Lv.%02d %s###Diff_%d",
                         opt.KeyCount, static_cast<int>(opt.Rating), opt.Name.c_str(), d);
              }

              if (isSelected) {
                ImGui::PushStyleColor(ImGuiCol_Button,
                                      ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
              }

              if (ImGui::Button(label,
                                ImVec2(ImGui::GetContentRegionAvail().x,
                                       MathUtil::ScaleVec2(0, 22).y))) {
                SelectDifficultyOption(d);
              }

              if (isSelected) {
                ImGui::PopStyleColor();
              }
            }
          }
          ImGui::EndChild();
        }
      }

      ImGui::Spacing();
      ImGui::PushItemWidth(-FLT_MIN);

      ImGui::Text("Note speed");
      {
        ImGui::InputFloat("###NoteSpeed", &currentSpeed, 0.05f, 0.1f, "%.2f");
        currentSpeed = std::clamp(currentSpeed, 0.1f, 4.0f);
      }

      ImGui::Text("Rate");
      {
        ImGui::InputFloat("###Rate", &currentRate, 0.05f, 0.1f, "%.2f");
        currentRate = std::clamp(currentRate, 0.5f, 2.0f);
      }

      ImGui::PopItemWidth();

      ImGui::Text("Modifier");

      float modBtnWidth =
          floorf((ImGui::GetContentRegionAvail().x -
                  ImGui::GetStyle().ItemSpacing.x) *
                 0.5f);

      for (int i = 0; i < Mods.size(); i++) {
        auto &mod = Mods[i];

        int value = EnvironmentSetup::GetInt(mod);
        if (value == 1) {
          ImGui::PushStyleVar(ImGuiStyleVar_Alpha,
                              ImGui::GetStyle().Alpha * 0.9f);
          ImVec4 color = ImGui::GetStyleColorVec4(ImGuiCol_Button);
          ImGui::PushStyleColor(
              ImGuiCol_Button,
              ImVec4(color.x * 1.2f, color.y * 1.2f, color.z * 1.2f, 1.0f));
        }

        if (ImGui::Button(mod.c_str(), ImVec2(modBtnWidth, 0))) {
          EnvironmentSetup::SetInt(mod, value == 1 ? 0 : 1);

          switch (i) {
          case 0:                                 // Mirror
            EnvironmentSetup::SetInt(Mods[1], 0); // Random
            EnvironmentSetup::SetInt(Mods[2], 0); // Panic
            EnvironmentSetup::SetInt(Mods[3], 0); // Rearrange
            break;
          case 1:                                 // Random
            EnvironmentSetup::SetInt(Mods[0], 0); // Mirror
            EnvironmentSetup::SetInt(Mods[2], 0); // Panic
            EnvironmentSetup::SetInt(Mods[3], 0); // Rearrange
            break;
          case 2:                                 // Panic
            EnvironmentSetup::SetInt(Mods[0], 0); // Mirror
            EnvironmentSetup::SetInt(Mods[1], 0); // Random
            EnvironmentSetup::SetInt(Mods[3], 0); // Rearrange
            break;
          case 3:                                 // Rearrange
            EnvironmentSetup::SetInt(Mods[0], 0); // Mirror
            EnvironmentSetup::SetInt(Mods[1], 0); // Random
            EnvironmentSetup::SetInt(Mods[2], 0); // Panic
            bOpenRearrange = true;                // Open Rearrange Window
            break;
          case 5:                                 // Hidden
            EnvironmentSetup::SetInt(Mods[6], 0); // Flashlight
            EnvironmentSetup::SetInt(Mods[7], 0); // Sudden
            break;
          case 6:                                 // Flashlight
            EnvironmentSetup::SetInt(Mods[5], 0); // Hidden
            EnvironmentSetup::SetInt(Mods[7], 0); // Sudden
            break;
          case 7:                                 // Sudden
            EnvironmentSetup::SetInt(Mods[5], 0); // Hidden
            EnvironmentSetup::SetInt(Mods[6], 0); // Flashlight
            break;
          }
        }

        if (value == 1) {
          ImGui::PopStyleVar();
          ImGui::PopStyleColor();
        }

        // NewLine after 2 button on SameLine
        if ((i + 1) % 2 == 1) {
          ImGui::SameLine();
        }
      }

      ImGui::Spacing();
      ImGui::PushItemWidth(-FLT_MIN);
      ImGui::Text("Arena");

      // select
      m_arenas = SkinManager::GetInstance()->GetArenas();
      int value = EnvironmentSetup::GetInt("Arena");
      if (value >= m_arenas.size())
        value = 0;
      if (ImGui::BeginCombo("###ComboBox1Arena", m_arenas[value].c_str(), 0)) {
        for (int i = 0; i < m_arenas.size(); i++) {
          bool is_selected = i == value;
          if (ImGui::Selectable(m_arenas[i].c_str(), is_selected)) {
            EnvironmentSetup::SetInt("Arena", i);
          }
        }

        ImGui::EndCombo();
      }

      ImGui::PopStyleVar();

      ImGui::PopItemWidth();

      ImGui::EndChild();
    }

    ImGui::PopStyleVar();

    ImGui::EndChild();
  }

  // get current cursor pos
  ImVec2 cursorPos = ImGui::GetCursorPos();

  ImGui::SameLine();
  auto size = ImVec2(250, 575);
  auto xPos = MathUtil::ScaleVec2(windowNextSz.x - size.x - 5, 0);

  ImGui::SetCursorPosX(xPos.x);

  const int MAX_ROWS = 18;

  if (ImGui::BeginChild("#SongSelectChild", MathUtil::ScaleVec2(size), true)) {
    static char search[256] = {};
    static char previous[256] = {};
    static int previousCategory = -2;

    const char *categories[] = {"All", "O2Jam", "osu!mania", "BMS"};
    const int catValues[] = {-1, 0, 1, 2};
    float tabWidth = floorf((ImGui::GetContentRegionAvail().x - (3 * ImGui::GetStyle().ItemSpacing.x)) / 4.0f);

    for (int c = 0; c < 4; c++) {
      if (c > 0) ImGui::SameLine();
      bool isSelectedCat = (m_selectedCategory == catValues[c]);
      if (isSelectedCat) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.45f, 0.75f, 1.0f));
      }
      if (ImGui::Button(categories[c], ImVec2(tabWidth, MathUtil::ScaleVec2(0, 24).y))) {
        if (m_selectedCategory != catValues[c]) {
          m_selectedCategory = catValues[c];
          m_musicList = music->FindQuery(search, m_selectedCategory);
          isScrolled = true;
        }
      }
      if (isSelectedCat) {
        ImGui::PopStyleColor();
      }
    }
    ImGui::Spacing();

    float sortControlsHeight = ImGui::GetTextLineHeightWithSpacing() + MathUtil::ScaleVec2(0, 24).y;
    float searchBoxHeight = MathUtil::ScaleVec2(0, 48).y + sortControlsHeight + ImGui::GetStyle().ItemSpacing.y;
    float listHeight = ImGui::GetContentRegionAvail().y - searchBoxHeight;

    if (ImGui::BeginChild("#SongSelectChild2",
                          ImVec2(0, listHeight), false,
                          ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
      ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0, 0));

      if (m_musicList.empty() || strcmp(search, previous) != 0 || previousCategory != m_selectedCategory) {
        memset(previous, 0, sizeof(previous));

        auto lists = music->FindQuery(search, m_selectedCategory);
        m_musicList = lists;
        SortSongs();

        isScrolled = true;
        strcpy(previous, search);
        previousCategory = m_selectedCategory;
      }

      for (int i = 0; i < m_musicList.size(); i++) {
        DB_MusicItem &item = m_musicList[i];

        std::string Id = "###Button" + std::to_string(i);
        bool isSelected = item.Id == index;

        ImVec4 previousColor = ImGui::GetStyleColorVec4(ImGuiCol_Button);

        if (isSelected) {
          ImGui::PushStyleColor(ImGuiCol_Button,
                                ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        }

        char buffer[256];
        if (item.Category == 0) {
          sprintf(buffer, "Lv.%03d | %s", item.Difficulty[currentDifficulty],
                  (char *)item.Title);
        } else {
          sprintf(buffer, "[%dK] | %s", item.KeyCount, (char *)item.Title);
        }

        std::string content = buffer + Id;

        if (ImGui::ButtonEx(
                content.c_str(),
                ImVec2(ImGui::GetContentRegionAvail().x,
                       MathUtil::ScaleVec2(0, 25).y),
                ImGuiButtonFlags_MouseButtonLeft |
                    ImGuiButtonFlags_MouseButtonRight)) {

          if (item.Id != index) {
            index = item.Id;
            bSelectNewSong = true;
          }
        }

        /*
                FIXME: Goddamit!, this is hack because for some reason
                ImGui::IsMouseClicked won't work if ImGui::ButtonEx handled it.
        */
        content += "_Context";
        if (ImGui::BeginPopupContextWindow((const char *)content.c_str())) {
          ImGui::CloseCurrentPopup();
          ImGui::EndPopup();

          bOpenSongContext = true;
        }

        if (isSelected) {
          ImGui::PopStyleColor();

          if (isScrolled) {
            isScrolled = false;

            ImGui::SetScrollHereY(0.5f);
          }
        }
      }
      isScrolled = false;

      ImGui::PopStyleVar();
      ImGui::EndChild();
    }

    ImGui::Separator();

    ImGui::Text("Sort by:");
    const char *sortModes[] = {"Default", "Level", "Name"};
    float sortBtnH = MathUtil::ScaleVec2(0, 22).y;
    float sortComboW = ImGui::GetContentRegionAvail().x - MathUtil::ScaleVec2(60, 0).x - ImGui::GetStyle().ItemSpacing.x;

    ImGui::PushItemWidth(sortComboW);
    if (ImGui::BeginCombo("###SortMode", sortModes[m_sortMode], 0)) {
      for (int s = 0; s < 3; s++) {
        bool isSelectedSort = (m_sortMode == s);
        if (ImGui::Selectable(sortModes[s], isSelectedSort)) {
          if (m_sortMode != s) {
            m_sortMode = s;
            auto lists = music->FindQuery(search, m_selectedCategory);
            m_musicList = lists;
            SortSongs();
            isScrolled = true;
          }
        }
      }
      ImGui::EndCombo();
    }
    ImGui::PopItemWidth();

    ImGui::SameLine();
    const char *orderLabel = m_sortAscending ? "Asc" : "Desc";
    if (ImGui::Button(orderLabel, ImVec2(MathUtil::ScaleVec2(60, 0).x, sortBtnH))) {
      m_sortAscending = !m_sortAscending;
      SortSongs();
      isScrolled = true;
    }
    if (ImGui::IsItemHovered()) {
      if (m_sortMode == 1) {
        ImGui::SetTooltip("%s (Difficulty / Level)", m_sortAscending ? "Small -> Big" : "Big -> Small");
      } else {
        ImGui::SetTooltip("%s", m_sortAscending ? "A -> Z (Ascending)" : "Z -> A (Descending)");
      }
    }

    ImGui::PushItemWidth(-FLT_MIN);
    ImGui::Text("Search:");
    ImGui::InputTextEx(
        "###Search", "Search by Title, Artist, Notecharter...", search,
        sizeof(search), ImVec2(0, 0), ImGuiInputTextFlags_AutoSelectAll);

    // if press Enter
    if (ImGui::IsKeyPressed(ImGuiKey_Enter)) {
      // set focus to InputText
      ImGui::SetKeyboardFocusHere();
    }

    ImGui::PopItemWidth();

    ImGui::EndChild();
  }

  ImGui::SetCursorPos(cursorPos);
  ImGui::PushFont(FontResources::GetButtonFont());

  if (ImGui::Button("Play", MathUtil::ScaleVec2(ImVec2(200, 60)))) {
    bPlay = true;
  }

  ImGui::PopFont();

  waitTime += static_cast<float>(delta);
  const double waitTimeDelay = 0.1;

  if (ImGui::IsKeyDown(ImGuiKey_UpArrow) && waitTime >= waitTimeDelay) {
    auto it =
        std::find_if(m_musicList.begin(), m_musicList.end(),
                     [&](const DB_MusicItem &a) { return a.Id == index; });

    if (--it >= m_musicList.begin()) {
      index = it->Id;
      bSelectNewSong = true;
      isScrolled = true;
    }

    waitTime = 0;
  }

  if (ImGui::IsKeyDown(ImGuiKey_DownArrow) && waitTime >= waitTimeDelay) {
    auto it =
        std::find_if(m_musicList.begin(), m_musicList.end(),
                     [&](const DB_MusicItem &a) { return a.Id == index; });

    if (++it < m_musicList.end()) {
      index = it->Id;
      bSelectNewSong = true;
      isScrolled = true;
    }

    waitTime = 0;
  }
}

void SongSelectScene::Update(double delta) {
  if (static_cast<int>(currentAlpha) != static_cast<int>(nextAlpha)) {
    double increment = (delta * 2) * 100;

    // compare it using epsilon
    if (std::abs(currentAlpha - nextAlpha) < FLT_EPSILON) {
      currentAlpha = nextAlpha;
    } else {
      if (currentAlpha < nextAlpha) {
        currentAlpha =
            std::clamp(currentAlpha + (float)increment, 0.0f, 100.0f);
      } else {
        currentAlpha =
            std::clamp(currentAlpha - (float)increment, 0.0f, 100.0f);
      }
    }
  }

  if (is_update_bgm) {
    m_bgm->Update(delta);
  }

  if (MsgBox::GetResult("DialogResetList") == 1) {
    auto db = GameDatabase::GetInstance();
    db->Reset();

    m_tempMusicLists = MusicListMaker::PrepareAll();

    scene_index = 1;
    m_tempMusicIndex = 0;
  }
}

void SongSelectScene::Input(double delta) {}

void SongSelectScene::OnMouseDown(const MouseState &state) {}

bool SongSelectScene::Attach() {
  SkinManager::GetInstance()->ReloadSkin();
  SceneManager::DisplayFade(0, [] {});

  EnvironmentSetup::SetInt("Score", 0);
  EnvironmentSetup::SetInt("Cool", 0);
  EnvironmentSetup::SetInt("Good", 0);
  EnvironmentSetup::SetInt("Bad", 0);
  EnvironmentSetup::SetInt("Miss", 0);
  EnvironmentSetup::SetInt("JamCombo", 0);
  EnvironmentSetup::SetInt("MaxJamCombo", 0);
  EnvironmentSetup::SetInt("Combo", 0);
  EnvironmentSetup::SetInt("MaxCombo", 0);
  EnvironmentSetup::SetInt("LNCombo", 0);
  EnvironmentSetup::SetInt("LNMaxCombo", 0);
  EnvironmentSetup::SetInt("Failed", 0);

  isScrolled = true;
  currentAlpha = 100;
  nextAlpha = 100;
  m_lastTime = 0;

  auto db = GameDatabase::GetInstance();
  bool music_reload_required = db->FindAll().size() == 0;

  if (music_reload_required) {
    scene_index = 1;

    m_tempMusicLists = MusicListMaker::PrepareAll();
    m_tempMusicIndex = 0;
  } else {
    if (EnvironmentSetup::GetInt("FileOpen") == 1) {
      index = -1;
      bSelectNewSong = true;
    } else if (index == -1) {
      bSelectNewSong = true;
      index = db->Random().Id;
    }
  }

  auto path = SkinManager::GetInstance()->GetPath();

  if (SceneManager::GetInstance()->GetCurrentSceneIndex() !=
      GameScene::MAINMENU) {
    Audio *bgm = AudioManager::GetInstance()->Get("BGM");
    if (!bgm) {
      auto audioPath = path / "Audio";
      if (!std::filesystem::exists(audioPath)) {
          audioPath = std::filesystem::current_path() / "Resources" / "Audio";
      }
      auto BGMPath = audioPath / "BGM.ogg";

      if (std::filesystem::exists(BGMPath)) {
        AudioManager::GetInstance()->Create("BGM", BGMPath, &bgm);
      }
    }

    if (bgm && !bgm->IsPlaying()) {
      bgm->SetVolume(100);
      bgm->Play(0, true);
    }
  }

  auto bgPath = path / "SongSelect" / "Background.png";
  if (!std::filesystem::exists(bgPath)) {
      bgPath = path / "Menu" / "Background.png";
  }
  if (std::filesystem::exists(bgPath) && !m_background) {
    GameWindow *wnd = GameWindow::GetInstance();

    m_background = std::make_unique<Texture2D>(bgPath);
    m_background->Size =
        UDim2::fromOffset(wnd->GetBufferWidth(), wnd->GetBufferHeight());
  }

  auto rateValue = EnvironmentSetup::Get("SongRate");
  auto noteValue = Configuration::Load("Gameplay", "Notespeed");

  for (int i = 0; i < Mods.size(); i++) {
    EnvironmentSetup::SetInt(Mods[i], 0);
  }
  std::string selectedMods = Configuration::Load("Gameplay", "Modifiers");
  std::istringstream iss(selectedMods);
  std::string mod;
  while (std::getline(iss, mod, ',')) {
    EnvironmentSetup::SetInt(mod, 1);
  }
  if (EnvironmentSetup::GetInt("ParameterAutoplay") == 1) {
    EnvironmentSetup::SetInt("Autoplay", 1);
  }

  m_arenas = SkinManager::GetInstance()->GetArenas();

  std::string arenaValue = Configuration::Load("Gameplay", "Arena");
  try {
    int arenaInt = std::stoi(arenaValue);
    if (arenaInt >= m_arenas.size())
      arenaInt = 0;
    EnvironmentSetup::SetInt("Arena", arenaInt);
  } catch (const std::invalid_argument &) {
    EnvironmentSetup::SetInt("Arena", 0);
  }

  try {
    currentRate = std::stof(rateValue.c_str());
  } catch (const std::invalid_argument &) {
    currentRate = 1.0f;
  }

  try {
    int value = std::stoi(noteValue.c_str());

    currentSpeed = std::clamp(static_cast<float>(value) / 100.0f, 0.1f, 4.0f);
  } catch (const std::invalid_argument &) {
    currentSpeed = 2.2f;
  }

  is_update_bgm = false;
  isWait = (index != -1 || EnvironmentSetup::GetInt("FileOpen") == 1);
  waitTime = 0;

  if (!m_bgm) {
    m_bgm = std::make_unique<BGMPreview>();
    m_bgm->OnReady([&](bool start) {
      Audio *bgm = AudioManager::GetInstance()->Get("BGM");
      if (start) {
        if (bgm) {
          bgm->FadeOut();
        }

        SDL_Delay(1500);
        if (m_bgm) {
          m_bgm->Play();
        }

        is_update_bgm = true;
      } else {
        if (bgm) {
          bgm->FadeIn();
        }

        is_update_bgm = false;
      }
    });
  }

  EnvironmentSetup::SetInt("FontIndex", ResolutionWindowScene::GAMEPLAY);

  is_departing = false;
  return true;
}

void SongSelectScene::RestartGame() {
  wchar_t moduleFileName[MAX_PATH];
  GetModuleFileNameW(NULL, moduleFileName, MAX_PATH);

  std::wstring filePath = EnvironmentSetup::GetPath("FILE");

  std::wstring commandLine =
      std::wstring(moduleFileName) + L" \"" + filePath + L"\"";
  STARTUPINFOW si = {sizeof(si)};
  PROCESS_INFORMATION pi;

  if (CreateProcessW(NULL, const_cast<wchar_t *>(commandLine.c_str()), NULL,
                     NULL, FALSE, 0, NULL, NULL, &si,
                     &pi)) { // Fix antivirus false detection
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    ExitProcess(0);
  }
}

bool SongSelectScene::Detach() {
  if (m_bgm) {
    m_bgm->Stop();
  }

  Audio *extBgm = AudioManager::GetInstance()->Get("ExternalBGM");
  if (extBgm) {
    extBgm->Stop();
  }

  if (SceneManager::GetInstance()->GetCurrentSceneIndex() !=
      GameScene::MAINMENU) {
    Audio *bgm = AudioManager::GetInstance()->Get("BGM");
    if (bgm) {
      bgm->Stop();
    }
  }

  isWait = false;

  return true;
}

void SongSelectScene::SaveConfiguration() {
  SaveModifiers();
  EnvironmentSetup::Set("SongRate", std::to_string(currentRate));
  Configuration::Set(
      "Gameplay", "Notespeed",
      std::to_string(static_cast<int>(::round(currentSpeed * 100.0))));
}

void SongSelectScene::SaveModifiers() {
  Configuration::Set("Gameplay", "Difficulty",
                     std::to_string(currentDifficulty));

  std::string selectedMods;
  for (int i = 0; i < Mods.size(); i++) {
    std::string mod = Mods[i];
    int value = EnvironmentSetup::GetInt(mod);
    if (value == 1) {
      if (!selectedMods.empty())
        selectedMods += ",";
      selectedMods += mod;
    }
  }
  Configuration::Set("Gameplay", "Modifiers", selectedMods);

  int arenaValue = EnvironmentSetup::GetInt("Arena");
  Configuration::Set("Gameplay", "Arena", std::to_string(arenaValue));
}

void SongSelectScene::LoadChartImage() {
  std::lock_guard<std::mutex> lock(m_imageLock);

  m_songBackground.reset();

  if (EnvironmentSetup::GetInt("FileOpen") == 1) {
    std::filesystem::path file = EnvironmentSetup::GetPath("FILE");
    if (m_externalFilePath != file) {
      m_externalFilePath = file;
      ParseExternalChart(file);
    }

    std::string ext = file.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    std::string bgFile = "";
    std::string audioFile = "";

    if (ext == ".osu") {
      Osu::Beatmap beatmap(file);
      if (beatmap.IsValid()) {
        if (!beatmap.BackgroundFile.empty()) {
          bgFile = (file.parent_path() / beatmap.BackgroundFile).string();
        }
        if (!beatmap.AudioFilename.empty()) {
          audioFile = (file.parent_path() / beatmap.AudioFilename).string();
        }
      }
    } else if (ext == ".bms" || ext == ".bme" || ext == ".bml" || ext == ".bmsc") {
      BMS::BMSFile bms;
      bms.Load(file);
      if (bms.IsValid() && !bms.StageFile.empty()) {
        bgFile = (file.parent_path() / bms.StageFile).string();
      }
      if (bgFile.empty() || !std::filesystem::exists(bgFile)) {
        std::error_code ec;
        std::filesystem::path bestImage;
        uintmax_t maxImageSize = 0;
        for (const auto &entry : std::filesystem::directory_iterator(file.parent_path(), ec)) {
          if (entry.is_regular_file(ec)) {
            std::string imgExt = entry.path().extension().string();
            std::transform(imgExt.begin(), imgExt.end(), imgExt.begin(), ::tolower);
            if (imgExt == ".png" || imgExt == ".jpg" || imgExt == ".jpeg" || imgExt == ".bmp") {
              auto sz = entry.file_size(ec);
              if (!ec && sz > maxImageSize) {
                maxImageSize = sz;
                bestImage = entry.path();
              }
            }
          }
        }
        if (maxImageSize > 0 && !bestImage.empty()) {
          bgFile = bestImage.string();
        }
      }
    } else if (ext == ".ojn") {
      O2::OJN ojn;
      ojn.Load(file, false);
      if (ojn.IsValid() && ojn.BackgroundImage.size() > 0) {
        GameWindow *wnd = GameWindow::GetInstance();
        try {
          m_songBackground = std::make_unique<Texture2D>(
              reinterpret_cast<uint8_t *>(ojn.BackgroundImage.data()),
              ojn.BackgroundImage.size());
          m_songBackground->Size =
              UDim2::fromOffset(wnd->GetBufferWidth(), wnd->GetBufferHeight());
        } catch (...) {
        }
      }
    }

    if (!bgFile.empty()) {
      GameWindow *wnd = GameWindow::GetInstance();
      try {
        m_songBackground = std::make_unique<Texture2D>(bgFile);
        m_songBackground->Size =
            UDim2::fromOffset(wnd->GetBufferWidth(), wnd->GetBufferHeight());
      } catch (...) {
      }
    }

    AudioManager::GetInstance()->Remove("ExternalBGM");
    if (!audioFile.empty()) {
      Audio *out = nullptr;
      AudioManager::GetInstance()->Create("ExternalBGM", audioFile, &out);
    }

    return;
  }

  if (index != -1) {
    DB_MusicItem item = GameDatabase::GetInstance()->Find(index);
    if (item.Id == -1) {
      MsgBox::Show("DialogErr", "Error", "LoadChartImage()::item null!");
      return;
    }

    std::filesystem::path file;
    if (item.FilePath[0] != '\0' && std::filesystem::exists(item.FilePath)) {
      file = item.FilePath;
    } else {
      file = GameDatabase::GetInstance()->GetPath() / ("o2ma" + std::to_string(item.Id) + ".ojn");
    }

    if (item.Category == 2) {
      std::filesystem::path bmsDir = file.parent_path();
      std::string bgFile;
      BMS::BMSFile bms;
      bms.Load(file);
      if (bms.IsValid() && !bms.StageFile.empty()) {
        bgFile = (bmsDir / bms.StageFile).string();
      }
      if (bgFile.empty() || !std::filesystem::exists(bgFile)) {
        std::error_code ec;
        std::filesystem::path bestImage;
        uintmax_t maxImageSize = 0;
        for (const auto &entry : std::filesystem::directory_iterator(bmsDir, ec)) {
          if (entry.is_regular_file(ec)) {
            std::string imgExt = entry.path().extension().string();
            std::transform(imgExt.begin(), imgExt.end(), imgExt.begin(), ::tolower);
            if (imgExt == ".png" || imgExt == ".jpg" || imgExt == ".jpeg" || imgExt == ".bmp") {
              auto sz = entry.file_size(ec);
              if (!ec && sz > maxImageSize) {
                maxImageSize = sz;
                bestImage = entry.path();
              }
            }
          }
        }
        if (maxImageSize > 0 && !bestImage.empty()) {
          bgFile = bestImage.string();
        }
      }

      if (!bgFile.empty()) {
        GameWindow *wnd = GameWindow::GetInstance();
        try {
          m_songBackground = std::make_unique<Texture2D>(bgFile);
          m_songBackground->Size =
              UDim2::fromOffset(wnd->GetBufferWidth(), wnd->GetBufferHeight());
        } catch (...) {
        }
      }
      return;
    }

    try {
      if (item.CoverSize < 1) {
        return;
      }

      GameWindow *wnd = GameWindow::GetInstance();

      auto buffer = O2::OJN::LoadOJNFile(file);
      buffer.seekg(item.CoverOffset);

      std::vector<uint8_t> coverData;
      coverData.resize(item.CoverSize);
      buffer.read((char *)coverData.data(), item.CoverSize);

      m_songBackground =
          std::make_unique<Texture2D>(coverData.data(), item.CoverSize);
      m_songBackground->Size =
          UDim2::fromOffset(wnd->GetBufferWidth(), wnd->GetBufferHeight());
    } catch (std::runtime_error &e) {
      MsgBox::Show("Selection_BgError", "Error", e.what());
    } catch (SDLException &e) {
      MsgBox::Show("Selection_BgError", "Error", e.what());
    }
  }
}

void SongSelectScene::LoadDifficultyOptions(const DB_MusicItem &item) {
  m_difficultyOptions.clear();
  m_selectedDiffIndex = 0;

  if (item.Category == 0) {
    return;
  }

  std::filesystem::path folder = std::filesystem::path(item.FilePath).parent_path();
  if (!std::filesystem::exists(folder)) {
    return;
  }

  std::error_code ec;
  if (item.Category == 1) {
    for (auto &entry : std::filesystem::directory_iterator(folder, ec)) {
      if (entry.is_regular_file()) {
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".osu") {
          std::filesystem::path p = entry.path();
          Osu::Beatmap bm(p);
          if (bm.IsValid() && bm.Mode == 3 && (int)bm.CircleSize >= 4 && (int)bm.CircleSize <= 7) {
            DifficultyOption opt;
            opt.Name = bm.Version.empty() ? entry.path().stem().string() : bm.Version;
            opt.KeyCount = (int)bm.CircleSize;
            opt.Rating = bm.OverallDifficulty;
            int totalNotes = 0;
            for (const auto &ho : bm.HitObjects) {
              totalNotes += (ho.Type == 128) ? 2 : 1;
            }
            opt.TotalNotes = totalNotes;
            opt.FilePath = entry.path();
            m_difficultyOptions.push_back(opt);
          }
        }
      }
    }
  } else if (item.Category == 2) {
    for (auto &entry : std::filesystem::directory_iterator(folder, ec)) {
      if (entry.is_regular_file()) {
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".bms" || ext == ".bme" || ext == ".bml" || ext == ".bmsc") {
          std::filesystem::path p = entry.path();
          BMS::BMSFile bms;
          bms.Load(p);
          if (bms.IsValid() && bms.KeyCount >= 4 && bms.KeyCount <= 7) {
            DifficultyOption opt;
            opt.Name = bms.SubArtist.empty() ? entry.path().stem().string() : bms.SubArtist;
            if (opt.Name.empty() || opt.Name == "-") {
              opt.Name = entry.path().stem().string();
            }
            opt.KeyCount = bms.KeyCount;
            opt.Rating = static_cast<float>(bms.Level);
            int totalNotes = 0;
            for (const auto &n : bms.Notes) {
              totalNotes += (n.EndTime != -1) ? 2 : 1;
            }
            opt.TotalNotes = totalNotes;
            opt.FilePath = entry.path();
            m_difficultyOptions.push_back(opt);
          }
        }
      }
    }
  }

  std::sort(m_difficultyOptions.begin(), m_difficultyOptions.end(),
            [](const DifficultyOption &a, const DifficultyOption &b) {
              if (a.KeyCount != b.KeyCount) {
                return a.KeyCount < b.KeyCount;
              }
              if (a.Rating != b.Rating) {
                return a.Rating < b.Rating;
              }
              return a.TotalNotes < b.TotalNotes;
            });

  int selected = 0;
  for (size_t i = 0; i < m_difficultyOptions.size(); i++) {
    if (m_difficultyOptions[i].FilePath == item.FilePath) {
      selected = static_cast<int>(i);
      break;
    }
  }
  m_selectedDiffIndex = selected;
  if (!m_difficultyOptions.empty()) {
    SelectDifficultyOption(selected);
  }
}

void SongSelectScene::SelectDifficultyOption(int optIndex) {
  if (optIndex < 0 || optIndex >= static_cast<int>(m_difficultyOptions.size())) {
    return;
  }
  m_selectedDiffIndex = optIndex;
  const auto &opt = m_difficultyOptions[optIndex];

  EnvironmentSetup::SetInt("FileOpen", 1);
  EnvironmentSetup::SetPath("FILE", opt.FilePath);
  EnvironmentSetup::SetInt("Difficulty", 0);

  ParseExternalChart(opt.FilePath);
  LoadChartImage();

  Audio *extBgm = AudioManager::GetInstance()->Get("ExternalBGM");
  if (extBgm) {
    extBgm->FadeOut();
  }
  if (m_bgm->IsPlaying()) {
    m_bgm->Stop();
  }

  std::string ext = opt.FilePath.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

  if (ext == ".ojn" || ext == ".bms" || ext == ".bme" || ext == ".bml" || ext == ".bmsc") {
    m_bgm->Load();
  } else {
    Audio *bgm = AudioManager::GetInstance()->Get("BGM");
    if (bgm) {
      bgm->SetVolume(100);
      bgm->FadeOut();
    }

    std::string currentFile = opt.FilePath.string();
    SceneManager::ExecuteAfter(1000, [currentFile]() {
      if (EnvironmentSetup::GetInt("FileOpen") == 1 &&
          EnvironmentSetup::GetPath("FILE").string() == currentFile) {
        Audio *extBgm = AudioManager::GetInstance()->Get("ExternalBGM");
        if (extBgm) {
          extBgm->SetVolume(100);
          extBgm->Play();
        }
      }
    });
  }
}

void SongSelectScene::ParseExternalChart(std::filesystem::path file) {
  m_externalItem = {};
  m_externalItem.Id = -1;

  std::string ext = file.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

  if (ext == ".osu") {
    Osu::Beatmap beatmap(file);
    if (beatmap.IsValid()) {
      std::string title = !beatmap.TitleUnicode.empty() ? beatmap.TitleUnicode : beatmap.Title;
      std::string artist = !beatmap.ArtistUnicode.empty() ? beatmap.ArtistUnicode : beatmap.Artist;
      snprintf((char *)m_externalItem.Title, sizeof(m_externalItem.Title), "%s",
               title.c_str());
      snprintf((char *)m_externalItem.Artist, sizeof(m_externalItem.Artist),
               "%s", artist.c_str());
      snprintf((char *)m_externalItem.Noter, sizeof(m_externalItem.Noter), "%s",
               beatmap.Creator.c_str());

      int totalNotes = 0;
      for (const auto &ho : beatmap.HitObjects) {
        if (ho.Type == 128) {
          totalNotes += 2;
        } else {
          totalNotes += 1;
        }
      }
      m_externalItem.MaxNotes[0] = totalNotes;
      m_externalItem.MaxNotes[1] = totalNotes;
      m_externalItem.MaxNotes[2] = totalNotes;
      m_externalItem.KeyCount = (int)beatmap.CircleSize;

      if (!beatmap.TimingPoints.empty()) {
        std::vector<Osu::OsuTimingPoint> bpms;
        for (auto &timing : beatmap.TimingPoints) {
          if (timing.Inherited == 0 || timing.BeatLength < 0) {
            continue;
          }
          bpms.push_back(timing);
        }

        if (bpms.empty()) {
          m_externalItem.BPM = 0;
        } else if (bpms.size() == 1) {
          m_externalItem.BPM = 60000.0f / bpms[0].BeatLength;
        } else {
          double lastTime = 0;
          if (!beatmap.HitObjects.empty()) {
            auto &lastObj = beatmap.HitObjects.back();
            lastTime =
                (lastObj.Type == 128) ? lastObj.EndTime : lastObj.StartTime;
          }

          std::unordered_map<float, int> durations;
          double currentLastTime = lastTime;

          for (int i = (int)bpms.size() - 1; i >= 0; i--) {
            auto &tp = bpms[i];

            if (tp.Offset > currentLastTime) {
              continue;
            }

            int duration = (int)(currentLastTime - (i == 0 ? 0 : tp.Offset));
            currentLastTime = tp.Offset;

            float bpmValue = 60000.0f / tp.BeatLength;

            if (durations.find(bpmValue) != durations.end()) {
              durations[bpmValue] += duration;
            } else {
              durations[bpmValue] = duration;
            }
          }

          if (durations.empty()) {
            m_externalItem.BPM = 60000.0f / bpms[0].BeatLength;
          } else {
            int currentDuration = 0;
            float currentBPM = 0.0f;

            for (auto &[bpm, duration] : durations) {
              if (duration > currentDuration) {
                currentDuration = duration;
                currentBPM = bpm;
              }
            }

            m_externalItem.BPM = currentBPM;
          }
        }
      } else {
        m_externalItem.BPM = 0;
      }
    }
  } else if (ext == ".bms" || ext == ".bme" || ext == ".bml") {
    BMS::BMSFile bms;
    bms.Load(file);
    if (bms.IsValid()) {
      snprintf((char *)m_externalItem.Title, sizeof(m_externalItem.Title), "%s",
               bms.Title.c_str());
      snprintf((char *)m_externalItem.Artist, sizeof(m_externalItem.Artist),
               "%s", bms.Artist.c_str());
      snprintf((char *)m_externalItem.Noter, sizeof(m_externalItem.Noter), "%s",
               bms.SubArtist.empty() ? "-" : bms.SubArtist.c_str());

      int totalNotes = 0;
      for (const auto &note : bms.Notes) {
        if (note.EndTime != -1) {
          totalNotes += 2;
        } else {
          totalNotes += 1;
        }
      }
      m_externalItem.MaxNotes[0] = totalNotes;
      m_externalItem.MaxNotes[1] = totalNotes;
      m_externalItem.MaxNotes[2] = totalNotes;

      m_externalItem.BPM = bms.BPM;
      m_externalItem.KeyCount = bms.KeyCount;
    }
  } else if (ext == ".ojn") {
    O2::OJN ojn;
    ojn.Load(file, false);
    if (ojn.IsValid()) {
      std::u8string title = EnsureUtf8(ojn.Header.title, sizeof(ojn.Header.title));
      std::u8string artist = EnsureUtf8(ojn.Header.artist, sizeof(ojn.Header.artist));
      std::u8string noter = EnsureUtf8(ojn.Header.noter, sizeof(ojn.Header.noter));

      snprintf((char *)m_externalItem.Title, sizeof(m_externalItem.Title), "%s",
               (const char *)title.c_str());
      snprintf((char *)m_externalItem.Artist, sizeof(m_externalItem.Artist),
               "%s", (const char *)artist.c_str());
      snprintf((char *)m_externalItem.Noter, sizeof(m_externalItem.Noter), "%s",
               (const char *)noter.c_str());

      m_externalItem.MaxNotes[0] = ojn.Header.note_count[0];
      m_externalItem.MaxNotes[1] = ojn.Header.note_count[1];
      m_externalItem.MaxNotes[2] = ojn.Header.note_count[2];

      m_externalItem.BPM = ojn.Header.bpm;
    }
  }
}

void SongSelectScene::SortSongs() {
  if (m_sortMode == 0) {
    return;
  }

  std::sort(m_musicList.begin(), m_musicList.end(),
            [this](const DB_MusicItem &a, const DB_MusicItem &b) {
              if (m_sortMode == 1) {
                // Sort by Level / Difficulty
                int diffA = (a.Category == 0) ? a.Difficulty[currentDifficulty] : a.Difficulty[0];
                int diffB = (b.Category == 0) ? b.Difficulty[currentDifficulty] : b.Difficulty[0];
                if (diffA != diffB) {
                  return m_sortAscending ? (diffA < diffB) : (diffA > diffB);
                }
              }

              // Sort by Name / Title (fallback for same level or when sortMode == 2)
              std::string titleA = (const char *)a.Title;
              std::string titleB = (const char *)b.Title;
              std::transform(titleA.begin(), titleA.end(), titleA.begin(), ::tolower);
              std::transform(titleB.begin(), titleB.end(), titleB.begin(), ::tolower);

              if (titleA != titleB) {
                return m_sortAscending ? (titleA < titleB) : (titleA > titleB);
              }
              return a.Id < b.Id;
            });
}

