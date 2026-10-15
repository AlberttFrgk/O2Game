#include "Settings.h"
#include <Configuration.h>
#include <Imgui/ImguiUtil.h>
#include <Imgui/imgui.h>
#include <Rendering/Renderer.h>
#include <Rendering/Window.h>
#include <SDL2/SDL.h>
#include <SceneManager.h>
#include <Texture/MathUtils.h>

#include "../../Data/Util/Util.hpp"
#include "../../Engine/SkinManager.hpp"
#include "../../EnvironmentSetup.hpp"
#include <algorithm>
#include <array>
#include <map>
#include <vector>

#if _WIN32
#include <windows.h>
#include <shobjidl.h>
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")

static std::string BrowseFolder() {
  std::string result = "";
  HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
  bool needUninit = SUCCEEDED(hr);

  IFileOpenDialog *pfd = nullptr;
  if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd)))) {
    DWORD dwOptions;
    if (SUCCEEDED(pfd->GetOptions(&dwOptions))) {
      pfd->SetOptions(dwOptions | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    }
    if (SUCCEEDED(pfd->Show(NULL))) {
      IShellItem *psi = nullptr;
      if (SUCCEEDED(pfd->GetResult(&psi))) {
        PWSTR pszPath = nullptr;
        if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH, &pszPath))) {
          int utf8Len = WideCharToMultiByte(CP_UTF8, 0, pszPath, -1, NULL, 0, NULL, NULL);
          if (utf8Len > 0) {
            std::string path(utf8Len - 1, 0);
            WideCharToMultiByte(CP_UTF8, 0, pszPath, -1, &path[0], utf8Len, NULL, NULL);
            result = path;
          }
          CoTaskMemFree(pszPath);
        }
        psi->Release();
      }
    }
    pfd->Release();
  }

  if (needUninit) {
    CoUninitialize();
  }
  return result;
}
#endif

SettingsOverlay::SettingsOverlay() {
  m_name = "Settings";
  m_position = MathUtil::ScaleVec2(450, 450);
  m_size = ImVec2(0, 0);

  for (auto &dir : std::filesystem::directory_iterator(
           std::filesystem::current_path() / "Skins")) {
    if (dir.is_directory()) {
      if (std::filesystem::exists(dir.path() / "GameSkin.ini")) {
        skins.push_back(dir.path().filename().string());
      }
    }
  }

  LoadConfiguration();
}

static std::map<int, std::string> Graphics = {
    {0, "OpenGL"},    {1, "Vulkan"},
#if _WIN32
    {2, "DirectX-9"}, {3, "DirectX-11"}, {4, "DirectX-12"},
#endif
#if __APPLE__
    {5, "Metal"},
#endif
};

static std::array<std::string, 4> LongNote = {"None", "Short", "Normal", "Long"};
static std::array<std::string, 3> SelectedBackground = {"Arena", "Song", "Disable"};
static std::array<std::string, 3> NoteSkin = {"Square", "Circle", "Custom"};
static std::vector<std::string> m_resolutions = {};

int currentFPSIndex = 0;
int customFPS = 0;

namespace {
int GetScreenRefreshRate() {
  SDL_DisplayMode displayMode;
  if (SDL_GetCurrentDisplayMode(0, &displayMode) != 0) {
    return 60;
  }
  return displayMode.refresh_rate;
}

std::vector<std::string> GetFpsOptions() {
  int refreshRate = GetScreenRefreshRate();
  std::vector<int> multipliers = {1, 2, 3, 4, 5, 6, 8, 10, 15, 20};
  std::vector<std::string> fpsOptions;

  fpsOptions.push_back("VSync");

  for (int multiplier : multipliers) {
    fpsOptions.push_back(std::to_string(refreshRate * multiplier));
  }
  fpsOptions.push_back("Unlimited");
  return fpsOptions;
}
} // namespace

void SettingsOverlay::Render(double delta) {
  auto &io = ImGui::GetIO();

  int sceneIndex = EnvironmentSetup::GetInt("Setting_SceneIndex");
  bool changeResolution = false;

  switch (sceneIndex) {
  case 1: {
    auto key = EnvironmentSetup::Get("Scene_KbKey");
    std::string keyCount = EnvironmentSetup::Get("Scene_KbLaneCount");
    std::string laneLabel =
        keyCount.empty() ? ("Lane " + key)
                         : (keyCount.substr(0, 1) + "K Lane " + key);
    ImGui::Text("%s", "Waiting for keybind input...");
    ImGui::Text("%s",
                ("Press any key to set " + laneLabel + " keybind").c_str());
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                       "Press Escape to cancel.");

    const Uint8 *keyState = SDL_GetKeyboardState(nullptr);

    for (ImGuiKey k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; k = static_cast<ImGuiKey>(k + 1)) {
      if (ImGui::IsKeyPressed(k, false)) {
        if (k == ImGuiKey_Escape) {
          EnvironmentSetup::SetInt("Setting_SceneIndex", 0);
          break;
        }

        int scancode = io.KeyMap[k];
        if (scancode <= 0 || scancode >= SDL_NUM_SCANCODES) {
          for (int sc = 1; sc < SDL_NUM_SCANCODES; sc++) {
            if (keyState && keyState[sc]) {
              scancode = sc;
              break;
            }
          }
        }

        if (scancode > 0 && scancode < SDL_NUM_SCANCODES) {
          const char *name = SDL_GetScancodeName(static_cast<SDL_Scancode>(scancode));
          if (name && name[0] != '\0') {
            EnvironmentSetup::SetInt("Setting_SceneIndex", 0);
            Configuration::Set("KeyMapping", keyCount + "Lane" + key, name);
            break;
          }
        }
      }
    }
    break;
  }

  case 2: {
    ImGui::Text("You sure you want reset the settings?");
    ImGui::Text("This will reset all settings to default.");

    bool done = false;

    if (ImGui::Button("Yes###ButtonPrompt1",
                      MathUtil::ScaleVec2(ImVec2(40, 0)))) {
      done = true;

      Configuration::ResetConfiguration();
      LoadConfiguration();
    }

    ImGui::SameLine();

    if (ImGui::Button("No###ButtonPrompt2",
                      MathUtil::ScaleVec2(ImVec2(40, 0)))) {
      done = true;
    }

    if (done) {
      EnvironmentSetup::SetInt("Setting_SceneIndex", 0);
    }
    break;
  }

  default: {
    if (ImGui::BeginChild("###ChildSettingWnd",
                          MathUtil::ScaleVec2(ImVec2(450, 260)))) {
      if (ImGui::BeginTabBar("OptionTabBar")) {
        if (ImGui::BeginTabItem("Inputs")) {
          ImGui::Text("Keys Configuration");
          for (int i = 0; i < 7; i++) {
            if (i != 0) {
              ImGui::SameLine();
            }

            std::string currentKey = Configuration::Load(
                "KeyMapping", "Lane" + std::to_string(i + 1));
            if (currentKey.empty()) {
              currentKey = "?";
            }
            if (ImGui::Button(
                    (currentKey + "###7KEY" + std::to_string(i)).c_str(),
                    MathUtil::ScaleVec2(ImVec2(50, 0)))) {
              EnvironmentSetup::SetInt("Setting_SceneIndex", 1);
              EnvironmentSetup::Set("Scene_KbLaneCount", "");
              EnvironmentSetup::Set("Scene_KbKey", std::to_string(i + 1));
            }
          }

          ImGui::NewLine();

          ImGui::Text("6 Keys Configuration");
          for (int i = 0; i < 6; i++) {
            if (i != 0) {
              ImGui::SameLine();
            }

            std::string currentKey = Configuration::Load(
                "KeyMapping", "6_Lane" + std::to_string(i + 1));
            if (currentKey.empty()) {
              currentKey = "?";
            }
            if (ImGui::Button(
                    (currentKey + "###6KEY" + std::to_string(i)).c_str(),
                    MathUtil::ScaleVec2(ImVec2(50, 0)))) {
              EnvironmentSetup::SetInt("Setting_SceneIndex", 1);
              EnvironmentSetup::Set("Scene_KbLaneCount", "6_");
              EnvironmentSetup::Set("Scene_KbKey", std::to_string(i + 1));
            }
          }

          ImGui::NewLine();

          ImGui::Text("5 Keys Configuration");
          for (int i = 0; i < 5; i++) {
            if (i != 0) {
              ImGui::SameLine();
            }

            std::string currentKey = Configuration::Load(
                "KeyMapping", "5_Lane" + std::to_string(i + 1));
            if (currentKey.empty()) {
              currentKey = "?";
            }
            if (ImGui::Button(
                    (currentKey + "###5KEY" + std::to_string(i)).c_str(),
                    MathUtil::ScaleVec2(ImVec2(50, 0)))) {
              EnvironmentSetup::SetInt("Setting_SceneIndex", 1);
              EnvironmentSetup::Set("Scene_KbLaneCount", "5_");
              EnvironmentSetup::Set("Scene_KbKey", std::to_string(i + 1));
            }
          }

          ImGui::NewLine();

          ImGui::Text("4 Keys Configuration");
          for (int i = 0; i < 4; i++) {
            if (i != 0) {
              ImGui::SameLine();
            }

            std::string currentKey = Configuration::Load(
                "KeyMapping", "4_Lane" + std::to_string(i + 1));
            if (currentKey.empty()) {
              currentKey = "?";
            }
            if (ImGui::Button(
                    (currentKey + "###4KEY" + std::to_string(i)).c_str(),
                    MathUtil::ScaleVec2(ImVec2(50, 0)))) {
              EnvironmentSetup::SetInt("Setting_SceneIndex", 1);
              EnvironmentSetup::Set("Scene_KbLaneCount", "4_");
              EnvironmentSetup::Set("Scene_KbKey", std::to_string(i + 1));
            }
          }

          ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Graphics")) {
          int GraphicsIndex = 0;
          int nextGraphicsIndex = -1;
          try {
            GraphicsIndex =
                std::stoi(Configuration::Load("Game", "Renderer").c_str());
          } catch (const std::invalid_argument &) {
          }

          if (Graphics.find(GraphicsIndex) == Graphics.end()) {
            GraphicsIndex = 0;
          }

          ImGui::Text("Graphics");
          ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f),
                             "You need restart game after setting this!");

          if (ImGui::BeginCombo("###ComboBox1",
                                Graphics[GraphicsIndex].c_str())) {
            for (const auto &[idx, name] : Graphics) {
              bool isSelected = (GraphicsIndex == idx);

              if (ImGui::Selectable(name.c_str(), &isSelected)) {
                nextGraphicsIndex = idx;
              }

              if (isSelected) {
                ImGui::SetItemDefaultFocus();
              }
            }

            ImGui::EndCombo();
          }

          if (nextGraphicsIndex != -1 && nextGraphicsIndex != GraphicsIndex) {
            Configuration::Set("Game", "Renderer",
                               std::to_string(nextGraphicsIndex));
          }

          int currentResolutionIndexRn = currentResolutionIndex;

          ImGui::Text("Resolution");
          if (!m_resolutions.empty()) {
            if (currentResolutionIndex < 0 || currentResolutionIndex >= static_cast<int>(m_resolutions.size())) {
              currentResolutionIndex = 0;
            }
            if (ImGui::BeginCombo(
                    "###ComboBox3",
                    m_resolutions[currentResolutionIndex].c_str())) {
              for (int i = 0; i < static_cast<int>(m_resolutions.size()); i++) {
                bool isSelected = (currentResolutionIndex == i);

                if (ImGui::Selectable(m_resolutions[i].c_str(), &isSelected)) {
                  currentResolutionIndex = i;
                }

                if (isSelected) {
                  ImGui::SetItemDefaultFocus();
                }
              }

              ImGui::EndCombo();
            }

            if (currentResolutionIndex != currentResolutionIndexRn) {
              changeResolution = true;
              Configuration::Set("Game", "Resolution",
                                 m_resolutions[currentResolutionIndex]);
            }
          }

          std::vector<std::string> fpsOptions = GetFpsOptions();
          if (currentFPSIndex < 0 || currentFPSIndex >= static_cast<int>(fpsOptions.size())) {
            currentFPSIndex = 0;
          }

          ImGui::Text("FPS");
          ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f),
                             "Warning: setting unlimited FPS can cause PC to "
                             "become unstable!");
          if (ImGui::BeginCombo("###ComboBox2",
                                fpsOptions[currentFPSIndex].c_str())) {
            for (int i = 0; i < static_cast<int>(fpsOptions.size()); i++) {
              bool isSelected = (currentFPSIndex == i);

              if (ImGui::Selectable(fpsOptions[i].c_str(), &isSelected)) {
                currentFPSIndex = i;
                std::string opt = fpsOptions[i];
                if (opt == "Unlimited") {
                  customFPS = 9999;
                  Configuration::Set("Game", "FrameLimit", "Unlimited");
                  SceneManager::GetInstance()->SetFrameLimit(9999.0);
                  Renderer::GetInstance()->SetVSync(false);
                } else if (opt == "VSync") {
                  customFPS = 0;
                  Configuration::Set("Game", "FrameLimit", "VSync");
                  SceneManager::GetInstance()->SetFrameLimit(0.0);
                  Renderer::GetInstance()->SetVSync(true);
                } else {
                  customFPS = 0;
                  Configuration::Set("Game", "FrameLimit", opt);
                  try {
                    SceneManager::GetInstance()->SetFrameLimit(std::stod(opt));
                  } catch (...) {
                    SceneManager::GetInstance()->SetFrameLimit(60.0);
                  }
                  Renderer::GetInstance()->SetVSync(false);
                }
              }

              if (isSelected) {
                ImGui::SetItemDefaultFocus();
              }
            }
            ImGui::EndCombo();
          }

          ImGui::NewLine();
          ImGui::Checkbox("Show FPS in-game", &ShowFPS);
          if (ShowFPS) {
            ImGui::SameLine();
            ImGui::Checkbox("Show Frame Latency (ms)", &ShowLatency);
          }

          ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Audio")) {
          ImGui::Text("Audio Volume");
          if (ImGui::SliderInt("###Slider2", &currentVolume, 0, 100)) {
            EnvironmentSetup::SetInt("AudioVolume", currentVolume);
          }

          ImGui::NewLine();

          ImGui::Text("Audio Offset");
          ImGui::TextColored(
              ImVec4(1.0f, 1.0f, 0.0f, 1.0f),
              "Warning: this will make keysounded sample as auto sample!");
          if (ImGui::SliderInt("###Slider1", &currentOffset, -1000, 1000)) {
            EnvironmentSetup::SetInt("AudioOffset", currentOffset);
          }

          ImGui::NewLine();
          if (ImGui::Checkbox("Convert Sample to Auto Sample###Checkbox1",
                              &convertAutoSound)) {
            EnvironmentSetup::SetInt("AutoSound", convertAutoSound ? 1 : 0);
          }
          ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f),
                             "Convert all keysounded sample to auto sample");

          ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Game")) {
          ImGui::Text("Background Dim");
          if (ImGui::SliderInt("###SliderDim", &BackgroundDim, 0, 100)) {
            EnvironmentSetup::SetInt("BackgroundDim", BackgroundDim);
          }
          if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Adjust background brightness (0%% = full brightness, 100%% = completely black)");
          }

          ImGui::NewLine();
          if (ImGui::Checkbox("Enable Video###CheckboxVideo", &LoadVideo)) {
            EnvironmentSetup::SetInt("LoadVideo", LoadVideo ? 1 : 0);
          }
          if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Load video from osu!mania beatmap");
          }

          ImGui::NewLine();
          ImGui::Text("Guide Line Length");

          for (int i = 0; i < static_cast<int>(LongNote.size()); i++) {
            if (i != 0) {
              ImGui::SameLine();
            }
            if (ImGui::RadioButton(LongNote[i].c_str(), currentGuideLineIndex == i)) {
              currentGuideLineIndex = i;
              EnvironmentSetup::SetInt("GuideLine", currentGuideLineIndex);
            }
          }

          ImGui::NewLine();

          ImGui::Text("Gameplay-Related Configuration");

          if (ImGui::Checkbox("Measure Line###SetCheckbox2", &MeasureLine)) {
            EnvironmentSetup::SetInt("MeasureLine", MeasureLine ? 1 : 0);
          }
          if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Show or hide measure lines during gameplay");
          }

          ImGui::SameLine();

          if (ImGui::Checkbox("New Measure Line###SetCheckbox1", &MeasureLineType)) {
            EnvironmentSetup::SetInt("MeasureLineType", MeasureLineType ? 1 : 0);
          }
          if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "Measure line position in the middle of the note; otherwise, "
                "it will be at the bottom of the note");
          }

          ImGui::SameLine();

          if (ImGui::Checkbox("New Long Note###SetCheckbox3", &NewLongNote)) {
            EnvironmentSetup::SetInt("NewLN", NewLongNote ? 1 : 0);
          }
          if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Use alternate long note rendering style");
          }

          ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Skins")) {
          ImGui::Text("Current selected skin: ");
          if (ImGui::BeginCombo("##SkinsComboBox", currentSkin.c_str())) {
            for (size_t i = 0; i < skins.size(); i++) {
              bool isSelected = (currentSkin == skins[i]);

              if (ImGui::Selectable(skins[i].c_str(), &isSelected)) {
                currentSkin = skins[i];
              }

              if (isSelected) {
                ImGui::SetItemDefaultFocus();
              }
            }

            ImGui::EndCombo();
          }

          ImGui::NewLine();

          ImGui::Text("Note Skin");
          for (int i = 0; i < static_cast<int>(NoteSkin.size()); ++i) {
            if (i != 0) {
              ImGui::SameLine();
            }

            if (ImGui::RadioButton(NoteSkin[i].c_str(), NoteIndex == i)) {
              NoteIndex = i;
              EnvironmentSetup::SetInt("NoteSkin", NoteIndex);
            }

            if (i == 2 && ImGui::IsItemHovered()) {
              ImGui::SetTooltip("Using custom note image from skin folder");
            }
          }

          ImGui::NewLine();
          ImGui::NewLine();

          ImGui::Text("Background");
          for (int i = 0; i < static_cast<int>(SelectedBackground.size()); ++i) {
            if (i != 0) {
              ImGui::SameLine();
            }

            if (ImGui::RadioButton(SelectedBackground[i].c_str(), BackgroundIndex == i)) {
              BackgroundIndex = i;
              EnvironmentSetup::SetInt("Background", BackgroundIndex);
              EnvironmentSetup::SetInt("Song Background", BackgroundIndex == 1 ? 1 : 0);
              EnvironmentSetup::SetInt("Song BG", BackgroundIndex == 1 ? 1 : 0);
              EnvironmentSetup::SetInt("Black Background", BackgroundIndex == 2 ? 1 : 0);
              EnvironmentSetup::SetInt("Black BG", BackgroundIndex == 2 ? 1 : 0);
            }

            const char *tooltipText = nullptr;
            if (i == 0) {
              tooltipText = "Using arena image background inside Skin folder";
            } else if (i == 1) {
              tooltipText = "Using song image background inside O2Jam file / BMS folder / osu!mania beatmap folder";
            } else if (i == 2) {
              tooltipText = "Not using any background";
            }

            if (tooltipText && ImGui::IsItemHovered()) {
              ImGui::SetTooltip("%s", tooltipText);
            }
          }

          ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Directories")) {
          ImGui::Text("Song Directories");
          ImGui::Spacing();

          ImGui::Text("O2Jam Folder (.ojn)");
          ImGui::InputText("###O2JamDir", m_o2jamDir, sizeof(m_o2jamDir));
          ImGui::SameLine();
          if (ImGui::Button("Browse...###BrowseO2Jam")) {
#if _WIN32
            std::string folder = BrowseFolder();
            if (!folder.empty()) {
              strncpy(m_o2jamDir, folder.c_str(), sizeof(m_o2jamDir) - 1);
            }
#endif
          }

          ImGui::Spacing();
          ImGui::Text("osu!mania Songs Folder (.osu)");
          ImGui::InputText("###OsuDir", m_osuDir, sizeof(m_osuDir));
          ImGui::SameLine();
          if (ImGui::Button("Browse...###BrowseOsu")) {
#if _WIN32
            std::string folder = BrowseFolder();
            if (!folder.empty()) {
              strncpy(m_osuDir, folder.c_str(), sizeof(m_osuDir) - 1);
            }
#endif
          }

          ImGui::Spacing();
          ImGui::Text("BMS Folder (.bms, .bme, .bml, .bmsc)");
          ImGui::InputText("###BMSDir", m_bmsDir, sizeof(m_bmsDir));
          ImGui::SameLine();
          if (ImGui::Button("Browse...###BrowseBMS")) {
#if _WIN32
            std::string folder = BrowseFolder();
            if (!folder.empty()) {
              strncpy(m_bmsDir, folder.c_str(), sizeof(m_bmsDir) - 1);
            }
#endif
          }

          ImGui::Spacing();
          ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Only 4K up to 7K charts are supported and scanned.");
          ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Press F5 in Song Select to rescan songs after updating.");

          ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
      }

      ImGui::EndChild();
    }

    ImGui::NewLine();

    if (ImGui::Button("Reset###Setting1", MathUtil::ScaleVec2(50, 0))) {
      EnvironmentSetup::SetInt("Setting_SceneIndex", 2);
    }

    ImGui::SameLine();

    if (ImGui::Button("OK###Setting2", MathUtil::ScaleVec2(50, 0))) {
      bSave = true;
      m_exit = true;
    }
    break;
  }
  }

  if (changeResolution && !m_resolutions.empty() && currentResolutionIndex >= 0 && currentResolutionIndex < static_cast<int>(m_resolutions.size())) {
    std::vector<std::string> resolution =
        splitString(m_resolutions[currentResolutionIndex], 'x');
    int x = 800, y = 600;

    try {
      x = std::stoi(resolution[0]);
      y = std::stoi(resolution[1]);
    } catch (const std::invalid_argument &) {
    }

    GameWindow::GetInstance()->ResizeWindow(x, y);
  }
}

bool SettingsOverlay::Attach() {
  LoadConfiguration();

  m_exit = false;
  return true;
}

bool SettingsOverlay::Detach() {
  ImGui::CloseCurrentPopup();
  SaveConfiguration();

  return true;
}

void SettingsOverlay::LoadConfiguration() {
  if (m_resolutions.empty()) {
    int displayCount = SDL_GetNumDisplayModes(0);
    for (int i = 0; i < displayCount; i++) {
      SDL_DisplayMode mode;
      SDL_GetDisplayMode(0, i, &mode);

      m_resolutions.push_back(std::to_string(mode.w) + "x" +
                              std::to_string(mode.h));
    }

    m_resolutions.erase(std::unique(m_resolutions.begin(), m_resolutions.end()),
                        m_resolutions.end());

    GameWindow *window = GameWindow::GetInstance();
    std::string currentResolution = std::to_string(window->GetWidth()) + "x" +
                                    std::to_string(window->GetHeight());

    auto it = std::find(m_resolutions.begin(), m_resolutions.end(), currentResolution);
    if (it != m_resolutions.end()) {
      currentResolutionIndex = static_cast<int>(std::distance(m_resolutions.begin(), it));
    } else {
      m_resolutions.push_back(currentResolution);
      currentResolutionIndex = static_cast<int>(m_resolutions.size()) - 1;
    }
  }

  try {
    currentOffset =
        std::stoi(Configuration::Load("Game", "AudioOffset").c_str());
  } catch (const std::invalid_argument &) {
    currentOffset = 0;
  }
  EnvironmentSetup::SetInt("AudioOffset", currentOffset);

  try {
    currentVolume =
        std::stoi(Configuration::Load("Game", "AudioVolume").c_str());
  } catch (const std::invalid_argument &) {
    currentVolume = 100;
  }
  EnvironmentSetup::SetInt("AudioVolume", currentVolume);

  try {
    convertAutoSound =
        std::stoi(Configuration::Load("Game", "AutoSound").c_str()) != 0;
  } catch (const std::invalid_argument &) {
    convertAutoSound = false;
  }
  EnvironmentSetup::SetInt("AutoSound", convertAutoSound ? 1 : 0);

  std::string frameLimit = Configuration::Load("Game", "FrameLimit");
  try {
    auto fpsOptions = GetFpsOptions();
    auto it = std::find(fpsOptions.begin(), fpsOptions.end(), frameLimit);
    if (it != fpsOptions.end()) {
      currentFPSIndex = static_cast<int>(std::distance(fpsOptions.begin(), it));
      customFPS = (frameLimit == "Unlimited") ? 9999 : 0;
    } else if (frameLimit == "9999") {
      auto unlimIt = std::find(fpsOptions.begin(), fpsOptions.end(), "Unlimited");
      if (unlimIt != fpsOptions.end()) {
        currentFPSIndex = static_cast<int>(std::distance(fpsOptions.begin(), unlimIt));
      } else {
        currentFPSIndex = static_cast<int>(fpsOptions.size()) - 1;
      }
      customFPS = 9999;
    } else {
      currentFPSIndex = static_cast<int>(fpsOptions.size()) - 1;
      customFPS = 9999;
    }
  } catch (const std::invalid_argument &) {
    currentFPSIndex = 0;
    customFPS = 0;
  }

  if (frameLimit == "VSync") {
    SceneManager::GetInstance()->SetFrameLimit(0.0);
    Renderer::GetInstance()->SetVSync(true);
  } else if (frameLimit == "Unlimited" || frameLimit == "9999") {
    SceneManager::GetInstance()->SetFrameLimit(9999.0);
    Renderer::GetInstance()->SetVSync(false);
  } else {
    try {
      SceneManager::GetInstance()->SetFrameLimit(std::stod(frameLimit));
    } catch (...) {
      SceneManager::GetInstance()->SetFrameLimit(60.0);
    }
    Renderer::GetInstance()->SetVSync(false);
  }

  try {
    currentGuideLineIndex =
        std::stoi(Configuration::Load("Game", "GuideLine").c_str());
  } catch (const std::invalid_argument &) {
    currentGuideLineIndex = 1;
  }
  if (currentGuideLineIndex < 0 || currentGuideLineIndex >= static_cast<int>(LongNote.size())) {
    currentGuideLineIndex = 1;
  }
  EnvironmentSetup::SetInt("GuideLine", currentGuideLineIndex);

  try {
    BackgroundIndex =
        std::stoi(Configuration::Load("Game", "Background").c_str());
  } catch (const std::invalid_argument &) {
    BackgroundIndex = 0;
  }
  if (BackgroundIndex < 0 || BackgroundIndex >= static_cast<int>(SelectedBackground.size())) {
    BackgroundIndex = 0;
  }
  EnvironmentSetup::SetInt("Background", BackgroundIndex);
  EnvironmentSetup::SetInt("Song Background", BackgroundIndex == 1 ? 1 : 0);
  EnvironmentSetup::SetInt("Song BG", BackgroundIndex == 1 ? 1 : 0);
  EnvironmentSetup::SetInt("Black Background", BackgroundIndex == 2 ? 1 : 0);
  EnvironmentSetup::SetInt("Black BG", BackgroundIndex == 2 ? 1 : 0);

  try {
    NoteIndex = std::stoi(Configuration::Load("Game", "NoteSkin").c_str());
  } catch (const std::invalid_argument &) {
    NoteIndex = 2;
  }
  if (NoteIndex < 0 || NoteIndex >= static_cast<int>(NoteSkin.size())) {
    NoteIndex = 2;
  }
  EnvironmentSetup::SetInt("NoteSkin", NoteIndex);

  try {
    int NewLNValue = std::stoi(Configuration::Load("Game", "NewLongNote"));
    NewLongNote = (NewLNValue == 1);
  } catch (const std::invalid_argument &) {
    NewLongNote = false;
  }
  EnvironmentSetup::SetInt("NewLN", NewLongNote ? 1 : 0);

  try {
    int MeasureLineTypeValue =
        std::stoi(Configuration::Load("Game", "MeasureLineType"));
    MeasureLineType = (MeasureLineTypeValue == 1);
  } catch (const std::invalid_argument &) {
    MeasureLineType = false;
  }
  EnvironmentSetup::SetInt("MeasureLineType", MeasureLineType ? 1 : 0);

  try {
    int measureLineValue =
        std::stoi(Configuration::Load("Game", "MeasureLine"));
    MeasureLine = (measureLineValue == 1);
  } catch (const std::invalid_argument &) {
    MeasureLine = true;
  }
  EnvironmentSetup::SetInt("MeasureLine", MeasureLine ? 1 : 0);

  try {
    BackgroundDim = std::stoi(Configuration::Load("Game", "BackgroundDim"));
  } catch (const std::invalid_argument &) {
    BackgroundDim = 50;
  }
  EnvironmentSetup::SetInt("BackgroundDim", BackgroundDim);

  try {
    LoadVideo = std::stoi(Configuration::Load("Game", "LoadVideo")) == 1;
  } catch (const std::invalid_argument &) {
    LoadVideo = false;
  }
  EnvironmentSetup::SetInt("LoadVideo", LoadVideo ? 1 : 0);

  try {
    ShowFPS = std::stoi(Configuration::Load("Game", "ShowFPS")) == 1;
  } catch (const std::invalid_argument &) {
    ShowFPS = false;
  }
  
  try {
    ShowLatency = std::stoi(Configuration::Load("Game", "ShowLatency")) == 1;
  } catch (const std::invalid_argument &) {
    ShowLatency = false;
  }

  currentSkin = Configuration::Load("Game", "Skin");
  if (currentSkin.empty()) {
    currentSkin = "Default";
  }

  std::string o2jam = Configuration::Load("Music", "O2Jam");
  if (o2jam.empty()) {
    o2jam = Configuration::Load("Music", "Folder");
  }
  strncpy(m_o2jamDir, o2jam.c_str(), sizeof(m_o2jamDir) - 1);

  std::string osu = Configuration::Load("Music", "Osu");
  strncpy(m_osuDir, osu.c_str(), sizeof(m_osuDir) - 1);

  std::string bms = Configuration::Load("Music", "BMS");
  strncpy(m_bmsDir, bms.c_str(), sizeof(m_bmsDir) - 1);

  PreloadSkin();
}

void SettingsOverlay::SaveConfiguration() {
  Configuration::Set("Game", "AudioOffset", std::to_string(currentOffset));
  Configuration::Set("Game", "AudioVolume", std::to_string(currentVolume));
  Configuration::Set("Game", "AutoSound", std::to_string(convertAutoSound ? 1 : 0));
  Configuration::Set("Game", "GuideLine", std::to_string(currentGuideLineIndex));
  Configuration::Set("Game", "NoteSkin", std::to_string(NoteIndex));
  Configuration::Set("Game", "Background", std::to_string(BackgroundIndex));
  Configuration::Set("Game", "MeasureLine", std::to_string(MeasureLine ? 1 : 0));
  Configuration::Set("Game", "MeasureLineType", std::to_string(MeasureLineType ? 1 : 0));

  Configuration::Set("Game", "NewLongNote", std::to_string(NewLongNote ? 1 : 0));
  Configuration::Set("Game", "BackgroundDim", std::to_string(BackgroundDim));
  Configuration::Set("Game", "LoadVideo", std::to_string(LoadVideo ? 1 : 0));
  Configuration::Set("Game", "ShowFPS", std::to_string(ShowFPS ? 1 : 0));
  Configuration::Set("Game", "ShowLatency", std::to_string(ShowLatency ? 1 : 0));

  EnvironmentSetup::SetInt("Background", BackgroundIndex);
  EnvironmentSetup::SetInt("Song Background", BackgroundIndex == 1 ? 1 : 0);
  EnvironmentSetup::SetInt("Song BG", BackgroundIndex == 1 ? 1 : 0);
  EnvironmentSetup::SetInt("Black Background", BackgroundIndex == 2 ? 1 : 0);
  EnvironmentSetup::SetInt("Black BG", BackgroundIndex == 2 ? 1 : 0);
  EnvironmentSetup::SetInt("NoteSkin", NoteIndex);
  EnvironmentSetup::SetInt("NewLN", NewLongNote ? 1 : 0);
  EnvironmentSetup::SetInt("MeasureLineType", MeasureLineType ? 1 : 0);
  EnvironmentSetup::SetInt("MeasureLine", MeasureLine ? 1 : 0);
  EnvironmentSetup::SetInt("BackgroundDim", BackgroundDim);
  EnvironmentSetup::SetInt("LoadVideo", LoadVideo ? 1 : 0);
  EnvironmentSetup::SetInt("GuideLine", currentGuideLineIndex);
  EnvironmentSetup::SetInt("AudioOffset", currentOffset);
  EnvironmentSetup::SetInt("AudioVolume", currentVolume);
  EnvironmentSetup::SetInt("AutoSound", convertAutoSound ? 1 : 0);

  auto fpsOptions = GetFpsOptions();
  std::string frameLimit = (currentFPSIndex >= 0 && currentFPSIndex < static_cast<int>(fpsOptions.size()))
                               ? fpsOptions[currentFPSIndex]
                               : "60";
  Configuration::Set("Game", "FrameLimit", frameLimit);

  if (frameLimit == "VSync") {
    SceneManager::GetInstance()->SetFrameLimit(0.0);
    Renderer::GetInstance()->SetVSync(true);
  } else if (frameLimit == "Unlimited" || frameLimit == "9999") {
    SceneManager::GetInstance()->SetFrameLimit(9999.0);
    Renderer::GetInstance()->SetVSync(false);
  } else {
    try {
      SceneManager::GetInstance()->SetFrameLimit(std::stod(frameLimit));
    } catch (...) {
      SceneManager::GetInstance()->SetFrameLimit(60.0);
    }
    Renderer::GetInstance()->SetVSync(false);
  }

  if (currentSkin.empty()) {
    currentSkin = "Default";
  }

  Configuration::Set("Game", "Skin", currentSkin);
  Configuration::Set("Music", "O2Jam", m_o2jamDir);
  Configuration::Set("Music", "Osu", m_osuDir);
  Configuration::Set("Music", "BMS", m_bmsDir);
  PreloadSkin();
}

void SettingsOverlay::PreloadSkin() {
  auto manager = SkinManager::GetInstance();
  manager->LoadSkin(currentSkin);

  std::string resolution =
      manager->GetSkinProp("Window", "NativeSize", "800x600");
  if (resolution.empty()) {
    resolution = "800x600";
  }

  std::vector<std::string> resolutionVec = splitString(resolution, 'x');
  int x = 800, y = 600;

  try {
    x = std::stoi(resolutionVec[0]);
    y = std::stoi(resolutionVec[1]);
  } catch (const std::invalid_argument &) {
  }

  GameWindow::GetInstance()->ResizeBuffer(x, y);
}
