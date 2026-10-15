#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "ResultScene.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "Audio/AudioManager.h"
#include "Configuration.h"
#include "Rendering/Window.h"
#include "SceneManager.h"
#include "Texture/MathUtils.h"

#include "Imgui/ImguiUtil.h"
#include "Imgui/imgui.h"
#include "Imgui/imgui_extends.h"
#include "Imgui/imgui_internal.h"

#include "Fonts/FontResources.h"

#include "../Data/Chart.hpp"
#include "../Engine/SkinManager.hpp"
#include "../EnvironmentSetup.hpp"
#include "../GameScenes.h"

static std::string FormatNumber(int value)
{
    std::string s = std::to_string(value);
    int n = (int)s.length() - 3;
    while (n > 0) {
        s.insert(n, ",");
        n -= 3;
    }
    return s;
}

ResultScene::ResultScene()
{
}

void ResultScene::Render(double delta)
{
    ImguiUtil::NewFrame();

    auto window = GameWindow::GetInstance();
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    if (displaySize.x <= 0 || displaySize.y <= 0) {
        displaySize = MathUtil::ScaleVec2(ImVec2((float)window->GetBufferWidth(), (float)window->GetBufferHeight()));
    }

    if (m_background) {
        m_background->Draw();
    }

    ImDrawList* bgDraw = ImGui::GetBackgroundDrawList();
    if (bgDraw) {
        bgDraw->AddRectFilled(ImVec2(0, 0), displaySize, IM_COL32(10, 14, 22, 191));
    }

    if (!m_animFinished) {
        m_animTime += delta;
        if (m_animTime >= 0.8) {
            m_animFinished = true;
        }
    }

    bool enterPressed = ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_Space, false);
    bool backPressed = ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsKeyPressed(ImGuiKey_Backspace, false);

    if (!m_animFinished) {
        if (enterPressed || backPressed || ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            m_animFinished = true;
        }
    } else if (!m_transitionStarted) {
        if (enterPressed) {
            m_retryButton = true;
        } else if (backPressed) {
            m_backButton = true;
        }
    }

    float t = m_animFinished ? 1.0f : (float)(m_animTime / 0.8);
    t = (std::clamp)(t, 0.0f, 1.0f);
    float ease = 1.0f - std::pow(1.0f - t, 3.0f);

    int displayScore = (int)(m_scoreData.Score * ease);
    int displayCool = (int)(m_scoreData.Cool * ease);
    int displayGood = (int)(m_scoreData.Good * ease);
    int displayBad = (int)(m_scoreData.Bad * ease);
    int displayMiss = (int)(m_scoreData.Miss * ease);
    int displayMaxCombo = (int)(m_scoreData.MaxCombo * ease);
    int displayMaxJam = (int)(m_scoreData.MaxJamCombo * ease);
    int displayMaxLN = (int)(m_scoreData.LNMaxCombo * ease);
    float displayAcc = m_scoreData.Accuracy * ease;
    float displaySongProgress = (std::clamp)(m_scoreData.SongProgress * ease, 0.0f, 1.0f);

    float padX = (std::clamp)(displaySize.x * 0.035f, 16.0f, 48.0f);
    float padY = (std::clamp)(displaySize.y * 0.035f, 12.0f, 36.0f);
    float panelH = displaySize.y - padY * 2.0f;
    float panelW = displaySize.x - padX * 2.0f;
    if (panelW > panelH * 1.777f) {
        panelW = panelH * 1.777f;
    }

    ImVec2 panelBaseSize = ImVec2(panelW, panelH);
    ImVec2 panelPos = ImVec2((displaySize.x - panelBaseSize.x) * 0.5f, (displaySize.y - panelBaseSize.y) * 0.5f);
    ImGui::SetNextWindowPos(panelPos);
    ImGui::SetNextWindowSize(panelBaseSize);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.10f, 0.15f, 0.75f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.24f, 0.32f, 0.44f, 0.70f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2((std::clamp)(panelW * 0.02f, 12.0f, 20.0f), (std::clamp)(panelH * 0.02f, 10.0f, 16.0f)));

    if (ImGui::Begin("#ResultDashboard",
                     nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus))
    {
        ImFont* btnFont = FontResources::GetButtonFont();

        if (btnFont) ImGui::PushFont(btnFont);
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", m_songData.Title.c_str());
        if (btnFont) ImGui::PopFont();

        ImGui::TextColored(ImVec4(0.68f, 0.74f, 0.82f, 1.0f), "Artist: %s", m_songData.Artist.c_str());

        std::string diffBadge;
        if (m_songData.Category == 1) {
            // osu!mania: show version/difficulty name and OD rating
            if (m_songData.Level > 0) {
                diffBadge = "[" + m_songData.DiffName + " OD " + std::to_string(m_songData.Level) + "]";
            } else {
                diffBadge = "[" + m_songData.DiffName + "]";
            }
        } else if (m_songData.Category == 2) {
            // BMS: show difficulty / subartist name and level
            if (m_songData.Level > 0) {
                diffBadge = "[" + m_songData.DiffName + " Lv." + std::to_string(m_songData.Level) + "]";
            } else {
                diffBadge = "[" + m_songData.DiffName + "]";
            }
        } else {
            // O2Jam: show EZ/NM/HD and Lv.
            diffBadge = "[" + m_songData.DiffName + " Lv." + std::to_string(m_songData.Level) + "]";
        }

        std::string metaBadges = diffBadge + "  [" +
                                 std::to_string(m_songData.KeyCount) + "K]  [" +
                                 std::to_string((int)::round(m_songData.BPM)) + " BPM]";
        if (m_songData.Modifiers != "None") {
            metaBadges += "  [Mods: " + m_songData.Modifiers + "]";
        }

        float metaWidth = ImGui::CalcTextSize(metaBadges.c_str()).x;
        float availX = ImGui::GetContentRegionAvail().x;
        if (availX > metaWidth) {
            ImGui::SameLine(ImGui::GetCursorPosX() + (availX - metaWidth));
        }
        ImGui::TextColored(ImVec4(0.40f, 0.75f, 1.0f, 1.0f), "%s", metaBadges.c_str());

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        float btnW = (std::clamp)(panelBaseSize.x * 0.16f, 130.0f, 180.0f);
        float btnH = (std::clamp)(panelBaseSize.y * 0.05f, 28.0f, 36.0f);
        float footerHeight = btnH + ImGui::GetStyle().ItemSpacing.y * 3.0f + 6.0f;
        float contentHeight = ImGui::GetContentRegionAvail().y - footerHeight;
        float totalWidth = ImGui::GetContentRegionAvail().x;
        float leftWidth = (std::clamp)(totalWidth * 0.35f, 260.0f, 440.0f);
        float rightWidth = totalWidth - leftWidth - ImGui::GetStyle().ItemSpacing.x;

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.06f, 0.08f, 0.12f, 0.65f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));

        // LEFT CARD: Clear Badge, Song Cover, Letter Grade, Score, Accuracy
        if (ImGui::BeginChild("#LeftSummaryCard", ImVec2(leftWidth, contentHeight), true,
                              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            ImDrawList* dl = ImGui::GetWindowDrawList();

            float bannerH = (std::clamp)(contentHeight * 0.065f, 24.0f, 30.0f);
            ImVec2 bannerSize = ImVec2(ImGui::GetContentRegionAvail().x, bannerH);
            ImVec2 b0 = ImGui::GetCursorScreenPos();
            ImVec2 b1 = ImVec2(b0.x + bannerSize.x, b0.y + bannerSize.y);
            dl->AddRectFilled(b0, b1, m_scoreData.ClearColor, 6.0f);

            ImVec2 textSz = ImGui::CalcTextSize(m_scoreData.ClearType.c_str());
            ImVec2 textP = ImVec2(b0.x + (bannerSize.x - textSz.x) * 0.5f, b0.y + (bannerSize.y - textSz.y) * 0.5f);
            dl->AddText(ImVec2(textP.x + 1, textP.y + 1), IM_COL32(0, 0, 0, 160), m_scoreData.ClearType.c_str());
            dl->AddText(textP, IM_COL32(255, 255, 255, 255), m_scoreData.ClearType.c_str());
            ImGui::Dummy(bannerSize);

            ImGui::Spacing();

            // Song cover stretched to container width without cropping
            float availW = ImGui::GetContentRegionAvail().x;
            float maxCoverH = (std::max)(contentHeight - 210.0f, 50.0f);
            float coverH = (std::min)(availW * (9.0f / 16.0f), maxCoverH);
            float coverW = availW;

            ImVec2 c0 = ImGui::GetCursorScreenPos();
            ImVec2 c1 = ImVec2(c0.x + coverW, c0.y + coverH);
            void* texId = m_background ? m_background->GetTextureID() : nullptr;

            if (texId != nullptr) {
                ImVec2 uv0(0.0f, 0.0f);
                ImVec2 uv1(1.0f, 1.0f);
                dl->AddImageRounded((ImTextureID)texId, c0, c1, uv0, uv1, IM_COL32_WHITE, 6.0f);
                dl->AddRect(c0, c1, IM_COL32(255, 255, 255, 60), 6.0f, 0, 1.0f);
            } else {
                dl->AddRectFilled(c0, c1, IM_COL32(18, 24, 34, 220), 6.0f);
                dl->AddRect(c0, c1, IM_COL32(45, 55, 75, 180), 6.0f, 0, 1.0f);
                ImVec2 tSz = ImGui::CalcTextSize(m_songData.Title.c_str());
                dl->AddText(ImVec2(c0.x + (coverW - tSz.x) * 0.5f, c0.y + (coverH - tSz.y) * 0.5f),
                            IM_COL32(160, 175, 195, 255), m_songData.Title.c_str());
            }
            ImGui::Dummy(ImVec2(availW, coverH));

            ImGui::Spacing();

            // Compact grade box with font scaling and visual glyph vertical centering
            float gradeH = (std::clamp)(contentHeight * 0.095f, 36.0f, 48.0f);
            ImVec2 gradeBoxSize = ImVec2(availW, gradeH);
            ImVec2 g0 = ImGui::GetCursorScreenPos();
            ImVec2 g1 = ImVec2(g0.x + gradeBoxSize.x, g0.y + gradeBoxSize.y);
            dl->AddRectFilled(g0, g1, IM_COL32(18, 24, 36, 220), 6.0f);
            dl->AddRect(g0, g1, m_scoreData.GradeColor, 6.0f, 0, 1.5f);

            ImFont* targetFont = btnFont ? btnFont : ImGui::GetFont();
            float fontBaseSize = targetFont ? targetFont->FontSize : 18.0f;
            float gradeFontSize = fontBaseSize * 1.8f;
            gradeFontSize = (std::min)(gradeFontSize, gradeH * 0.82f);
            ImVec2 gradeTextSz = targetFont ? targetFont->CalcTextSizeA(gradeFontSize, FLT_MAX, 0.0f, m_scoreData.Grade.c_str())
                                            : ImGui::CalcTextSize(m_scoreData.Grade.c_str());

            float gradeTextY = g0.y + (gradeBoxSize.y - gradeTextSz.y) * 0.5f;
            if (targetFont && targetFont->FontSize > 0.0f) {
                float minGlyphY = 0.0f;
                float maxGlyphY = 0.0f;
                bool hasGlyph = false;
                for (char c : m_scoreData.Grade) {
                    if (const ImFontGlyph* glyph = targetFont->FindGlyph((ImWchar)(unsigned char)c)) {
                        if (glyph->Visible) {
                            if (!hasGlyph) {
                                minGlyphY = glyph->Y0;
                                maxGlyphY = glyph->Y1;
                                hasGlyph = true;
                            } else {
                                minGlyphY = (std::min)(minGlyphY, glyph->Y0);
                                maxGlyphY = (std::max)(maxGlyphY, glyph->Y1);
                            }
                        }
                    }
                }
                if (hasGlyph && maxGlyphY > minGlyphY) {
                    float fontScale = gradeFontSize / targetFont->FontSize;
                    float visualCenterY = (minGlyphY + maxGlyphY) * 0.5f * fontScale;
                    gradeTextY = g0.y + (gradeBoxSize.y * 0.5f) - visualCenterY;
                }
            }

            ImVec2 gradeTextPos = ImVec2(g0.x + (gradeBoxSize.x - gradeTextSz.x) * 0.5f, gradeTextY);
            dl->AddText(targetFont, gradeFontSize, gradeTextPos, m_scoreData.GradeColor, m_scoreData.Grade.c_str());
            ImGui::Dummy(gradeBoxSize);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::TextColored(ImVec4(0.55f, 0.62f, 0.72f, 1.0f), "TOTAL SCORE");
            if (btnFont) ImGui::PushFont(btnFont);
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", FormatNumber(displayScore).c_str());
            if (btnFont) ImGui::PopFont();

            ImGui::Spacing();

            ImGui::TextColored(ImVec4(0.55f, 0.62f, 0.72f, 1.0f), "ACCURACY");
            char accStr[32];
            snprintf(accStr, sizeof(accStr), "%.2f%%", displayAcc);
            if (btnFont) ImGui::PushFont(btnFont);
            ImGui::TextColored(ImVec4(0.0f, 0.85f, 1.0f, 1.0f), "%s", accStr);
            if (btnFont) ImGui::PopFont();

            ImGui::EndChild();
        }

        ImGui::SameLine();

        // RIGHT CARD: Judgments Breakdown, Ratio Bar, Combo Summary, Hit Error Graph
        if (ImGui::BeginChild("#RightDetailsCard", ImVec2(rightWidth, contentHeight), true,
                              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            ImDrawList* dl = ImGui::GetWindowDrawList();

            if (btnFont) ImGui::PushFont(btnFont);
            ImGui::TextColored(ImVec4(0.9f, 0.95f, 1.0f, 1.0f), "JUDGMENT BREAKDOWN");
            if (btnFont) ImGui::PopFont();

            ImGui::Spacing();

            float cellPadY = (std::clamp)(contentHeight * 0.004f, 1.0f, 4.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4.0f, cellPadY));

            if (ImGui::BeginTable("#JudgmentTable", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH)) {
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Ratio", ImGuiTableColumnFlags_WidthFixed, 70.0f);

                auto AddRow = [&](const char* name, int count, ImVec4 color) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextColored(color, "%s", name);

                    ImGui::TableNextColumn();
                    if (btnFont) ImGui::PushFont(btnFont);
                    ImGui::Text("%s", FormatNumber(count).c_str());
                    if (btnFont) ImGui::PopFont();

                    ImGui::TableNextColumn();
                    float ratio = m_scoreData.TotalNotes > 0 ? (count * 100.0f / m_scoreData.TotalNotes) : 0.0f;
                    char ratioBuf[32];
                    snprintf(ratioBuf, sizeof(ratioBuf), "%.1f%%", ratio);
                    ImGui::TextColored(ImVec4(0.65f, 0.72f, 0.82f, 1.0f), "%s", ratioBuf);
                };

                AddRow("COOL", displayCool, ImVec4(0.0f, 0.90f, 1.0f, 1.0f));
                AddRow("GOOD", displayGood, ImVec4(0.0f, 0.90f, 0.46f, 1.0f));
                AddRow("BAD", displayBad, ImVec4(1.0f, 0.57f, 0.0f, 1.0f));
                AddRow("MISS", displayMiss, ImVec4(1.0f, 0.20f, 0.28f, 1.0f));

                ImGui::EndTable();
            }
            ImGui::PopStyleVar();

            ImGui::Spacing();

            // Song Progress Bar
            float barW = ImGui::GetContentRegionAvail().x;
            float barH = (std::clamp)(contentHeight * 0.016f, 7.0f, 10.0f);

            int curSec = (int)(m_scoreData.SongAudioPositionMs / 1000.0f);
            if (curSec < 0) curSec = 0;
            int durSec = (int)(m_scoreData.SongDurationMs / 1000.0f);
            if (durSec < 0) durSec = 0;

            char statusBuf[64];
            ImVec4 statusColor;
            if (m_scoreData.Failed) {
                float failPct = m_scoreData.SongProgress * 100.0f;
                if (durSec > 0) {
                    snprintf(statusBuf, sizeof(statusBuf), "%.1f%% (FAILED at %02d:%02d / %02d:%02d)",
                             failPct, curSec / 60, curSec % 60, durSec / 60, durSec % 60);
                } else {
                    snprintf(statusBuf, sizeof(statusBuf), "%.1f%% (FAILED)", failPct);
                }
                statusColor = ImVec4(1.0f, 0.35f, 0.40f, 1.0f);
            } else {
                if (durSec > 0) {
                    snprintf(statusBuf, sizeof(statusBuf), "100.0%% (CLEARED - %02d:%02d)", durSec / 60, durSec % 60);
                } else {
                    snprintf(statusBuf, sizeof(statusBuf), "100.0%% (CLEARED)");
                }
                statusColor = ImVec4(0.0f, 0.90f, 1.0f, 1.0f);
            }

            ImGui::TextColored(ImVec4(0.55f, 0.62f, 0.72f, 1.0f), "SONG PROGRESS");
            float statusW = ImGui::CalcTextSize(statusBuf).x;
            if (barW > statusW) {
                ImGui::SameLine(ImGui::GetCursorPosX() + (barW - statusW));
            }
            ImGui::TextColored(statusColor, "%s", statusBuf);

            ImVec2 b0 = ImGui::GetCursorScreenPos();
            ImVec2 b1 = ImVec2(b0.x + barW, b0.y + barH);

            dl->AddRectFilled(b0, b1, IM_COL32(20, 26, 36, 255), 3.0f);
            dl->AddRect(b0, b1, IM_COL32(40, 52, 70, 180), 3.0f, 0, 1.0f);

            float fillW = displaySongProgress * barW;
            if (fillW > 0.0f) {
                ImU32 fillCol = m_scoreData.Failed ? IM_COL32(235, 75, 75, 255) : IM_COL32(0, 229, 255, 255);
                ImU32 highlightCol = m_scoreData.Failed ? IM_COL32(255, 180, 180, 140) : IM_COL32(200, 250, 255, 140);

                dl->AddRectFilled(b0, ImVec2(b0.x + fillW, b1.y), fillCol, 3.0f);
                if (fillW > 4.0f) {
                    dl->AddLine(ImVec2(b0.x + 2.0f, b0.y + 1.0f), ImVec2(b0.x + fillW - 2.0f, b0.y + 1.0f), highlightCol, 1.0f);
                }

                if (m_scoreData.Failed && fillW < barW) {
                    float notchX = b0.x + fillW;
                    dl->AddLine(ImVec2(notchX, b0.y - 1.0f), ImVec2(notchX, b1.y + 1.0f), IM_COL32(255, 255, 255, 240), 2.0f);
                }
            }

            ImGui::Dummy(ImVec2(barW, barH));
            if (ImGui::IsItemHovered()) {
                if (m_scoreData.Failed) {
                    ImGui::SetTooltip("Song Progress: %.1f%% (%02d:%02d / %02d:%02d) - STAGE FAILED",
                                      m_scoreData.SongProgress * 100.0f,
                                      curSec / 60, curSec % 60, durSec / 60, durSec % 60);
                } else {
                    ImGui::SetTooltip("Song Progress: 100.0%% (%02d:%02d / %02d:%02d) - STAGE CLEAR",
                                      durSec / 60, durSec % 60, durSec / 60, durSec % 60);
                }
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // COMBO SUMMARY
            if (btnFont) ImGui::PushFont(btnFont);
            ImGui::TextColored(ImVec4(0.9f, 0.95f, 1.0f, 1.0f), "COMBO SUMMARY");
            if (btnFont) ImGui::PopFont();

            ImGui::Spacing();

            float comboAvailW = ImGui::GetContentRegionAvail().x;
            float comboSpacing = 8.0f;
            float boxW = (comboAvailW - comboSpacing * 2.0f) / 3.0f;
            float boxH = (std::clamp)(contentHeight * 0.09f, 36.0f, 48.0f);
            ImVec2 comboStartPos = ImGui::GetCursorScreenPos();

            struct ComboItem {
                const char* Title;
                int Value;
                ImU32 Color;
                ImU32 LabelColor;
            };

            ComboItem comboItems[3] = {
                { "MAX COMBO", displayMaxCombo, IM_COL32(255, 255, 255, 255), IM_COL32(140, 158, 184, 255) },
                { "MAX JAM",   displayMaxJam,   IM_COL32(255, 215, 0, 255),   IM_COL32(255, 215, 0, 200) },
                { "MAX LN",    displayMaxLN,    IM_COL32(80, 220, 255, 255),  IM_COL32(70, 200, 255, 200) }
            };

            for (int i = 0; i < 3; i++) {
                ImVec2 cardP0 = ImVec2(comboStartPos.x + i * (boxW + comboSpacing), comboStartPos.y);
                ImVec2 cardP1 = ImVec2(cardP0.x + boxW, cardP0.y + boxH);

                dl->AddRectFilled(cardP0, cardP1, IM_COL32(18, 24, 34, 200), 6.0f);
                dl->AddRect(cardP0, cardP1, IM_COL32(40, 52, 70, 180), 6.0f, 0, 1.0f);

                ImVec2 lblSz = ImGui::CalcTextSize(comboItems[i].Title);
                float lblTop = cardP0.y + (std::clamp)(boxH * 0.08f, 2.0f, 5.0f);
                dl->AddText(ImVec2(cardP0.x + (boxW - lblSz.x) * 0.5f, lblTop),
                            comboItems[i].LabelColor, comboItems[i].Title);

                std::string numStr = FormatNumber(comboItems[i].Value);
                float numFontSize = btnFont ? btnFont->FontSize : 18.0f;
                numFontSize = (std::min)(numFontSize, boxH * 0.44f);
                ImVec2 numSz = btnFont ? btnFont->CalcTextSizeA(numFontSize, FLT_MAX, 0.0f, numStr.c_str())
                                       : ImGui::CalcTextSize(numStr.c_str());
                float numBottom = cardP1.y - numSz.y - (std::clamp)(boxH * 0.08f, 2.0f, 5.0f);
                dl->AddText(btnFont, numFontSize,
                            ImVec2(cardP0.x + (boxW - numSz.x) * 0.5f, numBottom),
                            comboItems[i].Color, numStr.c_str());
            }
            ImGui::Dummy(ImVec2(comboAvailW, boxH));

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // HIT ERROR GRAPH
            float totalH = (float)(m_scoreData.HitRecords.empty() ? m_scoreData.HitOffsets.size() : m_scoreData.HitRecords.size());
            char hitsCap[32];
            snprintf(hitsCap, sizeof(hitsCap), "Hits: %d", (int)totalH);

            if (btnFont) ImGui::PushFont(btnFont);
            ImGui::TextColored(ImVec4(0.9f, 0.95f, 1.0f, 1.0f), "HIT ERROR GRAPH");
            if (btnFont) ImGui::PopFont();

            float hitsW = ImGui::CalcTextSize(hitsCap).x;
            float availHeaderW = ImGui::GetContentRegionAvail().x;
            if (availHeaderW > hitsW) {
                ImGui::SameLine(ImGui::GetCursorPosX() + (availHeaderW - hitsW));
                ImGui::TextColored(ImVec4(0.70f, 0.78f, 0.88f, 1.0f), "%s", hitsCap);
            }

            ImGui::Spacing();

            // Hit error graph canvas dynamically sized to fill remaining height
            float graphW = ImGui::GetContentRegionAvail().x;
            float captionLineH = ImGui::GetTextLineHeightWithSpacing() + 4.0f;
            float itemSpacingY = ImGui::GetStyle().ItemSpacing.y;
            float availH = ImGui::GetContentRegionAvail().y;
            float graphH = availH - captionLineH - itemSpacingY;
            graphH = (std::max)(graphH, 45.0f);

            ImVec2 gP0 = ImGui::GetCursorScreenPos();
            ImVec2 gP1 = ImVec2(gP0.x + graphW, gP0.y + graphH);

            dl->AddRectFilled(gP0, gP1, IM_COL32(10, 14, 22, 230), 4.0f);
            dl->AddRect(gP0, gP1, IM_COL32(35, 45, 62, 200), 4.0f, 0, 1.0f);

            dl->PushClipRect(gP0, gP1, true);

            float const rangeMs = 180.0f;
            float centerY = gP0.y + graphH * 0.5f;

            // Subtle judgment background bands
            float coolRangeY = (41.0f / rangeMs) * (graphH * 0.5f - 4.0f);
            dl->AddRectFilled(ImVec2(gP0.x, centerY - coolRangeY),
                              ImVec2(gP1.x, centerY + coolRangeY),
                              IM_COL32(255, 255, 255, 12));

            float goodRangeY = (80.0f / rangeMs) * (graphH * 0.5f - 4.0f);
            dl->AddRectFilled(ImVec2(gP0.x, centerY - goodRangeY),
                              ImVec2(gP1.x, centerY + goodRangeY),
                              IM_COL32(255, 225, 60, 10));

            // Centerline at 0ms (red/pink dashed line)
            for (float dx = gP0.x; dx < gP1.x; dx += 8.0f) {
                dl->AddLine(ImVec2(dx, centerY),
                            ImVec2((std::min)(dx + 4.0f, gP1.x), centerY),
                            IM_COL32(255, 110, 130, 160), 1.0f);
            }

            // Song timeline duration
            float songDuration = m_scoreData.SongDurationMs;
            if (songDuration <= 0.0f && !m_scoreData.HealthRecords.empty()) {
                songDuration = m_scoreData.HealthRecords.back().TimeMs;
            }
            if (songDuration <= 0.0f && !m_scoreData.HitRecords.empty()) {
                songDuration = m_scoreData.HitRecords.back().TimeMs;
            }
            if (songDuration <= 0.0f) {
                songDuration = 1.0f;
            }

            // Hit error scatter points colorized by judgment
            if (!m_scoreData.HitRecords.empty()) {
                for (const auto& hr : m_scoreData.HitRecords) {
                    float tRel = (std::clamp)(hr.TimeMs / songDuration, 0.0f, 1.0f);
                    float px = gP0.x + tRel * (graphW - 6.0f) + 3.0f;
                    float py = centerY;
                    ImU32 dotCol;

                    if (hr.Result == 3) {
                        py = gP1.y - 4.0f;
                        dotCol = IM_COL32(255, 45, 65, 230);
                    } else {
                        float clampedOff = (std::clamp)(hr.OffsetMs, -rangeMs, rangeMs);
                        py = centerY - (clampedOff / rangeMs) * (graphH * 0.5f - 6.0f);

                        if (hr.Result == 0) {
                            dotCol = IM_COL32(0, 229, 255, 230);
                        } else if (hr.Result == 1) {
                            dotCol = IM_COL32(0, 230, 118, 230);
                        } else if (hr.Result == 2) {
                            dotCol = IM_COL32(255, 145, 0, 230);
                        } else {
                            dotCol = IM_COL32(255, 45, 65, 230);
                        }
                    }
                    dl->AddCircleFilled(ImVec2(px, py), 2.0f, dotCol);
                }
            } else if (!m_scoreData.HitOffsets.empty()) {
                size_t n = m_scoreData.HitOffsets.size();
                for (size_t i = 0; i < n; i++) {
                    float tRel = n > 1 ? ((float)i / (float)(n - 1)) : 0.5f;
                    float px = gP0.x + tRel * (graphW - 6.0f) + 3.0f;
                    float off = m_scoreData.HitOffsets[i];
                    float clampedOff = (std::clamp)(off, -rangeMs, rangeMs);
                    float py = centerY - (clampedOff / rangeMs) * (graphH * 0.5f - 6.0f);

                    float absOff = std::abs(off);
                    ImU32 dotCol;
                    if (absOff <= 41.0f) {
                        dotCol = IM_COL32(0, 229, 255, 230);
                    } else if (absOff <= 80.0f) {
                        dotCol = IM_COL32(0, 230, 118, 230);
                    } else if (absOff <= 120.0f) {
                        dotCol = IM_COL32(255, 145, 0, 230);
                    } else {
                        dotCol = IM_COL32(255, 45, 65, 230);
                    }

                    dl->AddCircleFilled(ImVec2(px, py), 2.0f, dotCol);
                }
            }

            dl->AddText(ImVec2(gP0.x + 6, gP0.y + 3), IM_COL32(220, 230, 245, 200), "Late (+180ms)");
            dl->AddText(ImVec2(gP0.x + 6, centerY - 6), IM_COL32(255, 255, 255, 170), "0ms");
            float earlyLblY = gP1.y - ImGui::GetTextLineHeight() - 3.0f;
            if (earlyLblY > centerY + 4.0f) {
                dl->AddText(ImVec2(gP0.x + 6, earlyLblY), IM_COL32(220, 230, 245, 200), "Early (-180ms)");
            }

            dl->PopClipRect();

            ImGui::Dummy(ImVec2(graphW, graphH));

            ImGui::Spacing();

            // Caption under graph
            float ePct = totalH > 0 ? (m_scoreData.EarlyCount * 100.0f / totalH) : 0.0f;
            float lPct = totalH > 0 ? (m_scoreData.LateCount * 100.0f / totalH) : 0.0f;
            float urVal = m_scoreData.HitOffsetStdDev * 10.0f;

            char leftCap[48], midCap[64], rightCap[48];
            snprintf(leftCap, sizeof(leftCap), "Early: %d (%.1f%%)", m_scoreData.EarlyCount, ePct);
            snprintf(midCap, sizeof(midCap), "Avg: %+.1f ms  (UR: %.1f)", m_scoreData.MeanHitOffset, urVal);
            snprintf(rightCap, sizeof(rightCap), "Late: %d (%.1f%%)", m_scoreData.LateCount, lPct);

            float contentAvailW = ImGui::GetContentRegionAvail().x;
            float lineStartX = ImGui::GetCursorPosX();
            float lineStartY = ImGui::GetCursorPosY();

            // Early: left-aligned
            ImGui::SetCursorPos(ImVec2(lineStartX, lineStartY));
            ImGui::TextColored(ImVec4(0.0f, 0.85f, 1.0f, 1.0f), "%s", leftCap);

            // Avg: centered across the entire graph width
            float midSz = ImGui::CalcTextSize(midCap).x;
            ImGui::SetCursorPos(ImVec2(lineStartX + (contentAvailW - midSz) * 0.5f, lineStartY));
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "%s", midCap);

            // Late: right-aligned to the edge
            float rightSz = ImGui::CalcTextSize(rightCap).x;
            ImGui::SetCursorPos(ImVec2(lineStartX + contentAvailW - rightSz, lineStartY));
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "%s", rightCap);

            ImGui::EndChild();
        }

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImVec2 btnSize(btnW, btnH);

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.42f, 0.78f, 0.90f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.52f, 0.92f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.10f, 0.35f, 0.68f, 1.0f));
        if (ImGui::Button("Retry [Enter]", btnSize)) {
            m_retryButton = true;
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.25f, 0.32f, 0.85f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.35f, 0.45f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.16f, 0.20f, 0.26f, 1.0f));
        if (ImGui::Button("Song Select [Esc]", btnSize)) {
            m_backButton = true;
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        const char* hintText = "(Press Enter or Space to retry, Esc to song select)";
        float hintSz = ImGui::CalcTextSize(hintText).x;
        if (ImGui::GetContentRegionAvail().x > hintSz + 8.0f) {
            ImGui::TextColored(ImVec4(0.5f, 0.58f, 0.68f, 0.9f), "%s", hintText);
        }

        ImGui::End();
    }

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);

    if (m_backButton && !m_transitionStarted) {
        m_transitionStarted = true;
        SceneManager::DisplayFade(100, [] {
            SceneManager::ChangeScene(GameScene::SONGSELECT);
        });
    }

    if (m_retryButton && !m_transitionStarted) {
        m_retryButton = true;
        m_transitionStarted = true;
        SceneManager::DisplayFade(100, [] {
            SceneManager::ChangeScene(GameScene::LOADING);
        });
    }
}

bool ResultScene::Attach()
{
    SkinManager::GetInstance()->ReloadSkin();
    SceneManager::DisplayFade(0, [] {});
    m_backButton = m_retryButton = m_transitionStarted = false;
    m_animTime = 0.0;
    m_animFinished = false;
    m_scoreData = ResultScoreData();
    m_songData = ResultSongData();

    m_scoreData.Score = EnvironmentSetup::GetInt("Score");
    m_scoreData.Cool = EnvironmentSetup::GetInt("Cool");
    m_scoreData.Good = EnvironmentSetup::GetInt("Good");
    m_scoreData.Bad = EnvironmentSetup::GetInt("Bad");
    m_scoreData.Miss = EnvironmentSetup::GetInt("Miss");
    m_scoreData.TotalNotes = m_scoreData.Cool + m_scoreData.Good + m_scoreData.Bad + m_scoreData.Miss;

    m_scoreData.JamCombo = EnvironmentSetup::GetInt("JamCombo");
    m_scoreData.MaxJamCombo = EnvironmentSetup::GetInt("MaxJamCombo");
    m_scoreData.Combo = EnvironmentSetup::GetInt("Combo");
    m_scoreData.MaxCombo = EnvironmentSetup::GetInt("MaxCombo");
    m_scoreData.LNCombo = EnvironmentSetup::GetInt("LNCombo");
    m_scoreData.LNMaxCombo = EnvironmentSetup::GetInt("LNMaxCombo");

    m_scoreData.Failed = EnvironmentSetup::GetInt("Failed") == 1;

    // Retrieve Hit Error Offsets, Hit Records, and Health Records from gameplay
    auto* offsets = (std::vector<float>*)EnvironmentSetup::GetObj("HitOffsets");
    if (offsets) {
        m_scoreData.HitOffsets = *offsets;
        delete offsets;
        EnvironmentSetup::SetObj("HitOffsets", nullptr);
    } else {
        m_scoreData.HitOffsets.clear();
    }

    auto* hitRecs = (std::vector<HitRecord>*)EnvironmentSetup::GetObj("HitRecords");
    if (hitRecs) {
        m_scoreData.HitRecords = *hitRecs;
        delete hitRecs;
        EnvironmentSetup::SetObj("HitRecords", nullptr);
    } else {
        m_scoreData.HitRecords.clear();
    }

    auto* healthRecs = (std::vector<HealthRecord>*)EnvironmentSetup::GetObj("HealthRecords");
    if (healthRecs) {
        m_scoreData.HealthRecords = *healthRecs;
        delete healthRecs;
        EnvironmentSetup::SetObj("HealthRecords", nullptr);
    } else {
        m_scoreData.HealthRecords.clear();
    }

    auto* songLen = (double*)EnvironmentSetup::GetObj("SongAudioLength");
    if (songLen) {
        m_scoreData.SongDurationMs = (float)*songLen;
        delete songLen;
        EnvironmentSetup::SetObj("SongAudioLength", nullptr);
    } else {
        m_scoreData.SongDurationMs = 0.0f;
    }

    auto* songPos = (double*)EnvironmentSetup::GetObj("SongAudioPosition");
    if (songPos) {
        m_scoreData.SongAudioPositionMs = (float)*songPos;
        delete songPos;
        EnvironmentSetup::SetObj("SongAudioPosition", nullptr);
    } else {
        m_scoreData.SongAudioPositionMs = 0.0f;
    }

    auto* songProg = (double*)EnvironmentSetup::GetObj("SongProgress");
    if (songProg) {
        m_scoreData.SongProgress = (float)*songProg;
        delete songProg;
        EnvironmentSetup::SetObj("SongProgress", nullptr);
    } else {
        m_scoreData.SongProgress = m_scoreData.Failed ? 0.0f : 1.0f;
    }

    m_scoreData.EarlyCount = 0;
    m_scoreData.LateCount = 0;
    m_scoreData.PerfectCount = 0;
    m_scoreData.MeanHitOffset = 0.0f;
    m_scoreData.HitOffsetStdDev = 0.0f;

    if (!m_scoreData.HitOffsets.empty()) {
        double sum = 0.0;
        for (float off : m_scoreData.HitOffsets) {
            sum += off;
            if (off < -4.0f) {
                m_scoreData.EarlyCount++;
            } else if (off > 4.0f) {
                m_scoreData.LateCount++;
            } else {
                m_scoreData.PerfectCount++;
            }
        }
        m_scoreData.MeanHitOffset = (float)(sum / m_scoreData.HitOffsets.size());

        double sumSq = 0.0;
        for (float off : m_scoreData.HitOffsets) {
            double diff = off - m_scoreData.MeanHitOffset;
            sumSq += diff * diff;
        }
        m_scoreData.HitOffsetStdDev = (float)std::sqrt(sumSq / m_scoreData.HitOffsets.size());
    }

    if (m_scoreData.TotalNotes > 0) {
        m_scoreData.Accuracy = ((m_scoreData.Cool * 1.0f + m_scoreData.Good * 0.7f + m_scoreData.Bad * 0.2f) / (float)m_scoreData.TotalNotes) * 100.0f;
    } else {
        m_scoreData.Accuracy = 0.0f;
    }

    if (m_scoreData.Failed) {
        m_scoreData.Grade = "F";
        m_scoreData.GradeColor = IM_COL32(231, 76, 60, 255);
    } else if (m_scoreData.Accuracy >= 100.0f || (m_scoreData.TotalNotes > 0 && m_scoreData.Cool == m_scoreData.TotalNotes)) {
        m_scoreData.Grade = "SS";
        m_scoreData.GradeColor = IM_COL32(255, 215, 0, 255);
    } else if (m_scoreData.Accuracy >= 95.0f) {
        m_scoreData.Grade = "S";
        m_scoreData.GradeColor = IM_COL32(0, 229, 255, 255);
    } else if (m_scoreData.Accuracy >= 90.0f) {
        m_scoreData.Grade = "A";
        m_scoreData.GradeColor = IM_COL32(46, 204, 113, 255);
    } else if (m_scoreData.Accuracy >= 80.0f) {
        m_scoreData.Grade = "B";
        m_scoreData.GradeColor = IM_COL32(52, 152, 219, 255);
    } else if (m_scoreData.Accuracy >= 70.0f) {
        m_scoreData.Grade = "C";
        m_scoreData.GradeColor = IM_COL32(243, 156, 18, 255);
    } else {
        m_scoreData.Grade = "D";
        m_scoreData.GradeColor = IM_COL32(155, 89, 182, 255);
    }

    if (m_scoreData.Failed) {
        m_scoreData.ClearType = "STAGE FAILED";
        m_scoreData.ClearColor = IM_COL32(231, 76, 60, 255);
    } else if (m_scoreData.TotalNotes > 0 && m_scoreData.Cool == m_scoreData.TotalNotes) {
        m_scoreData.ClearType = "ALL COOL";
        m_scoreData.ClearColor = IM_COL32(255, 215, 0, 255);
    } else if (m_scoreData.TotalNotes > 0 && m_scoreData.Bad == 0 && m_scoreData.Miss == 0) {
        m_scoreData.ClearType = "FULL COMBO";
        m_scoreData.ClearColor = IM_COL32(0, 229, 255, 255);
    } else {
        m_scoreData.ClearType = "STAGE CLEAR";
        m_scoreData.ClearColor = IM_COL32(46, 204, 113, 255);
    }

    Audio* previousAudio = AudioManager::GetInstance()->Get("FINISH");
    if (previousAudio) {
        previousAudio->Stop();
        AudioManager::GetInstance()->Remove("FINISH");
    }

    Audio* audio = nullptr;
    auto SkinBGMPath = SkinManager::GetInstance()->GetPath() / "Audio";
    auto DefaultBGMPath = std::filesystem::current_path() / "Resources" / "Audio";

    std::string bgmName = m_scoreData.Failed ? "FAILED.ogg" : "FINISH.ogg";
    auto BGMPath = SkinBGMPath / bgmName;
    if (!std::filesystem::exists(BGMPath)) {
        BGMPath = DefaultBGMPath / bgmName;
        if (!std::filesystem::exists(BGMPath) && m_scoreData.Failed) {
            BGMPath = SkinBGMPath / "FINISH.ogg";
        }
    }

    if (std::filesystem::exists(BGMPath)) {
        AudioManager::GetInstance()->Create("FINISH", BGMPath, &audio);
        if (audio) {
            audio->SetVolume(100);
            audio->Play();
        }
    }

    Chart* chart = (Chart*)EnvironmentSetup::GetObj("SONG");
    EnvironmentSetup::SetObj("SONG", nullptr);

    if (m_scoreData.SongDurationMs <= 0.0f && chart) {
        m_scoreData.SongDurationMs = (float)chart->GetLength();
    }
    if (m_scoreData.SongDurationMs <= 0.0f && !m_scoreData.HitRecords.empty()) {
        m_scoreData.SongDurationMs = m_scoreData.HitRecords.back().TimeMs;
    }
    if (m_scoreData.SongDurationMs <= 0.0f && !m_scoreData.HealthRecords.empty()) {
        m_scoreData.SongDurationMs = m_scoreData.HealthRecords.back().TimeMs;
    }

    if (m_scoreData.Failed) {
        if (m_scoreData.SongAudioPositionMs <= 0.0f) {
            for (const auto& hr : m_scoreData.HealthRecords) {
                if (hr.Health <= 0.0f) {
                    m_scoreData.SongAudioPositionMs = hr.TimeMs;
                    break;
                }
            }
            if (m_scoreData.SongAudioPositionMs <= 0.0f && !m_scoreData.HealthRecords.empty()) {
                m_scoreData.SongAudioPositionMs = m_scoreData.HealthRecords.back().TimeMs;
            }
            if (m_scoreData.SongAudioPositionMs <= 0.0f && !m_scoreData.HitRecords.empty()) {
                m_scoreData.SongAudioPositionMs = m_scoreData.HitRecords.back().TimeMs;
            }
        }
        if (m_scoreData.SongDurationMs > 0.0f && m_scoreData.SongAudioPositionMs > 0.0f) {
            m_scoreData.SongProgress = (std::clamp)(m_scoreData.SongAudioPositionMs / m_scoreData.SongDurationMs, 0.0f, 1.0f);
        }
    } else {
        m_scoreData.SongProgress = 1.0f;
        if (m_scoreData.SongAudioPositionMs <= 0.0f) {
            m_scoreData.SongAudioPositionMs = m_scoreData.SongDurationMs;
        }
    }

    m_background = (Texture2D*)EnvironmentSetup::GetObj("SongBackground");

    if (chart) {
        if (!chart->m_title.empty()) {
            m_songData.Title = std::string((const char*)chart->m_title.c_str());
        } else {
            m_songData.Title = "Unknown Title";
        }

        if (!chart->m_artist.empty()) {
            m_songData.Artist = std::string((const char*)chart->m_artist.c_str());
        } else {
            m_songData.Artist = "Unknown Artist";
        }

        int songType = EnvironmentSetup::GetInt("SongType");
        m_songData.KeyCount = chart->m_keyCount > 0 ? chart->m_keyCount : 7;
        m_songData.Level = chart->m_level;
        m_songData.BPM = chart->BaseBPM;

        if (songType == 0) {
            // BMS
            m_songData.Category = 2;
            std::string subName = std::string((const char*)chart->m_difname.c_str());
            if (!subName.empty()) {
                m_songData.DiffName = subName;
            } else {
                m_songData.DiffName = "BMS";
            }
        } else if (songType == 2) {
            // osu!mania
            m_songData.Category = 1;
            std::string subName = std::string((const char*)chart->m_difname.c_str());
            if (!subName.empty()) {
                m_songData.DiffName = subName;
            } else {
                m_songData.DiffName = "Mania";
            }
        } else {
            // O2Jam
            m_songData.Category = 0;
            int diff = EnvironmentSetup::GetInt("Difficulty");
            std::string diffPrefix = (diff == 0) ? "EZ" : (diff == 1) ? "NM" : "HD";
            if (!chart->m_difname.empty()) {
                std::string subName = std::string((const char*)chart->m_difname.c_str());
                m_songData.DiffName = diffPrefix + " (" + subName + ")";
            } else {
                m_songData.DiffName = diffPrefix;
            }
        }

        delete chart;
    }

    std::string mods;
    if (EnvironmentSetup::GetInt("Autoplay") == 1) mods += "Autoplay, ";
    if (EnvironmentSetup::GetInt("Hidden") == 1) mods += "Hidden, ";
    if (EnvironmentSetup::GetInt("Flashlight") == 1) mods += "Flashlight, ";
    if (EnvironmentSetup::GetInt("Sudden") == 1) mods += "Sudden, ";
    if (EnvironmentSetup::GetInt("Random") == 1) mods += "Random, ";
    if (EnvironmentSetup::GetInt("Mirror") == 1) mods += "Mirror, ";
    if (EnvironmentSetup::GetInt("Panic") == 1) mods += "Panic, ";
    if (!mods.empty()) {
        mods.erase(mods.size() - 2);
    } else {
        mods = "None";
    }
    m_songData.Modifiers = mods;

    return true;
}

bool ResultScene::Detach()
{
    Audio* audio = AudioManager::GetInstance()->Get("FINISH");
    if (audio) {
        audio->Stop();
        AudioManager::GetInstance()->Remove("FINISH");
    }

    if (!m_retryButton) {
        EnvironmentSetup::SetObj("SongBackground", nullptr);
    }

    EnvironmentSetup::SetInt("Failed", 0);
    EnvironmentSetup::SetInt("FillStart", 0);

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

    auto* offsets = (std::vector<float>*)EnvironmentSetup::GetObj("HitOffsets");
    if (offsets) {
        delete offsets;
        EnvironmentSetup::SetObj("HitOffsets", nullptr);
    }

    auto* hitRecs = (std::vector<HitRecord>*)EnvironmentSetup::GetObj("HitRecords");
    if (hitRecs) {
        delete hitRecs;
        EnvironmentSetup::SetObj("HitRecords", nullptr);
    }

    auto* healthRecs = (std::vector<HealthRecord>*)EnvironmentSetup::GetObj("HealthRecords");
    if (healthRecs) {
        delete healthRecs;
        EnvironmentSetup::SetObj("HealthRecords", nullptr);
    }

    auto* songLen = (double*)EnvironmentSetup::GetObj("SongAudioLength");
    if (songLen) {
        delete songLen;
        EnvironmentSetup::SetObj("SongAudioLength", nullptr);
    }

    m_scoreData = ResultScoreData();
    m_songData = ResultSongData();
    m_background = nullptr;
    return true;
}