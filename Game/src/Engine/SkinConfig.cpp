#include "SkinConfig.hpp"

#include <filesystem>
#include <iostream>

#include "../Data/Util/Util.hpp"
#include "Misc/mINI.h"

SkinConfig::SkinConfig(std::string filePath, int keyCount)
{
    auto path = std::filesystem::current_path().string();

    if (!filePath.starts_with(path)) {
        filePath = path + filePath;
    }

    Load(path, keyCount);
}

SkinConfig::SkinConfig(std::filesystem::path path, int keyCount)
{
    Load(path, keyCount);
}

void SkinConfig::Load(std::filesystem::path path, int keyCount)
{
    std::filesystem::path current = path.parent_path();
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("File does not exist");
    }

    mINI::INIFile      f(path);
    mINI::INIStructure ini;
    f.read(ini);

    for (auto const &[key, value] : ini["Numerics"]) {
        auto rows = splitString(value, '|');
        for (auto &value2 : rows) {
            auto split = splitString(value2, ',');

            NumericValue e = {};
            e.X = std::stoi(split[0]);
            e.Y = std::stoi(split[1]);

            if (split.size() > 2) {
                e.MaxDigit = std::stoi(split[2]);
            } else {
                e.MaxDigit = 0;
                e.Direction = 0;
                e.FillWithZero = false;
            }

            if (split.size() > 3) {
                auto direction = split[3];
                std::transform(direction.begin(), direction.end(), direction.begin(), ::tolower);

                if (direction == "mid")
                    e.Direction = 0;
                else if (direction == "left")
                    e.Direction = -1;
                else if (direction == "right")
                    e.Direction = 1;
            } else {
                e.Direction = 0;
                e.FillWithZero = false;
            }

            if (split.size() > 4) {
                auto direction = split[4];
                std::transform(direction.begin(), direction.end(), direction.begin(), ::tolower);

                if (direction == "true")
                    e.FillWithZero = true;
                else if (direction == "false")
                    e.FillWithZero = false;
                else if (direction == "1")
                    e.FillWithZero = true;
                else
                    e.FillWithZero = false;
            } else {
                e.FillWithZero = false;
            }

            m_numericValues[key].push_back(std::move(e));
        }
    }

    std::string posName = "positions";
    std::string keyName = "Key";
    if (keyCount != -1) {
        posName += "#" + std::to_string(keyCount);
        keyName += "#" + std::to_string(keyCount);
    }

    // Parse legacy Positions
    for (auto const &[key, value] : ini[posName]) {
        auto rows = splitString(value, '|');
        for (auto &value2 : rows) {
            auto split = splitString(value2, ',');

            PositionValue e = {};
            e.X = std::stoi(split[0]);
            e.Y = std::stoi(split[1]);

            if (split.size() > 2) {
                e.AnchorPointX = std::stof(split[2]);
            }

            if (split.size() > 3) {
                e.AnchorPointY = std::stof(split[3]);
            }

            if (split.size() > 4) {
                if (split[4].find(':') != std::string::npos) {
                    auto splitRGB = splitString(split[4], ':');
                    if (splitRGB.size() >= 3) {
                        e.RGB[0] = std::stoi(splitRGB[0]);
                        e.RGB[1] = std::stoi(splitRGB[1]);
                        e.RGB[2] = std::stoi(splitRGB[2]);
                    }
                } else {
                    e.FPS = std::stof(split[4]);
                }
            }

            if (split.size() > 5) {
                if (split[5].find(':') != std::string::npos) {
                    auto splitRGB = splitString(split[5], ':');
                    if (splitRGB.size() >= 3) {
                        e.RGB[0] = std::stoi(splitRGB[0]);
                        e.RGB[1] = std::stoi(splitRGB[1]);
                        e.RGB[2] = std::stoi(splitRGB[2]);
                    }
                }
            }

            m_positionValues[key].push_back(std::move(e));
        }
    }

    for (auto const &[key, value] : ini[keyName]) {
        auto rows = splitString(value, '|');
        for (auto &value2 : rows) {
            auto split = splitString(value2, ',');

            PositionValue e = {};
            e.X = std::stoi(split[0]);
            e.Y = std::stoi(split[1]);

            if (split.size() > 2) {
                e.AnchorPointX = std::stof(split[2]);
            }

            if (split.size() > 3) {
                e.AnchorPointY = std::stof(split[3]);
            }

            if (split.size() > 4) {
                if (split[4].find(':') != std::string::npos) {
                    auto splitRGB = splitString(split[4], ':');
                    if (splitRGB.size() >= 3) {
                        e.RGB[0] = std::stoi(splitRGB[0]);
                        e.RGB[1] = std::stoi(splitRGB[1]);
                        e.RGB[2] = std::stoi(splitRGB[2]);
                    }
                } else {
                    e.FPS = std::stof(split[4]);
                }
            }

            if (split.size() > 5) {
                if (split[5].find(':') != std::string::npos) {
                    auto splitRGB = splitString(split[5], ':');
                    if (splitRGB.size() >= 3) {
                        e.RGB[0] = std::stoi(splitRGB[0]);
                        e.RGB[1] = std::stoi(splitRGB[1]);
                        e.RGB[2] = std::stoi(splitRGB[2]);
                    }
                }
            }

            m_positionValues[key].push_back(std::move(e));
        }
    }

    std::string keyName1 = "notes";
    if (keyCount != -1) {
        keyName1 += "#" + std::to_string(keyCount);
    }

    for (auto const &[key, value] : ini[keyName1]) {
        auto split = splitString(value, ',');

        NoteValue e = {};
        e.numOfFiles = std::stoi(split[0]);
        e.fileName = split[1];

        m_noteValues[key] = std::move(e);
    }

    for (auto const &[key, value] : ini["Sprites"]) {
        auto split = splitString(value, ',');

        SpriteValue e = {};
        e.X = std::stoi(split[0]);
        e.Y = std::stoi(split[1]);

        if (split.size() > 2) {
            e.AnchorPointX = std::stof(split[2]);
        }

        if (split.size() > 3) {
            e.AnchorPointY = std::stof(split[3]);
        }

        if (split.size() > 4) {
            e.FrameTime = std::stof(split[4]);
        }

        m_spriteValues[key] = std::move(e);
    }

    for (auto const &[key, value] : ini["Effects"]) {
        auto split = splitString(value, ',');

        SpriteValue e = {};
        e.X = std::stoi(split[0]);
        e.Y = std::stoi(split[1]);
        
        // Effects default to 0.5, 0.5 AnchorPoint as they are centered on the lane
        e.AnchorPointX = 0.5f;
        e.AnchorPointY = 0.5f;

        // Effects format: X, Y, FPS?, TintColor?
        if (split.size() > 2) {
            e.FrameTime = std::stof(split[2]);
        } else {
            e.FrameTime = 30.0f; // Default FPS for effects
        }

        m_spriteValues[key] = std::move(e);
    }

    for (auto const &[key, value] : ini["UI"]) {
        auto rows = splitString(value, '|');
        for (auto &value2 : rows) {
            auto split = splitString(value2, ',');

            PositionValue p = {};
            p.X = std::stoi(split[0]);
            p.Y = std::stoi(split[1]);

            RectInfo r = {};
            r.X = std::stoi(split[0]);
            r.Y = std::stoi(split[1]);

            if (split.size() > 2) {
                float v = std::stof(split[2]);
                if (v <= 1.0f) {
                    p.AnchorPointX = v;
                } else {
                    r.Width = std::stoi(split[2]);
                }
            }

            if (split.size() > 3) {
                float v = std::stof(split[3]);
                if (v <= 1.0f) {
                    p.AnchorPointY = v;
                } else {
                    r.Height = std::stoi(split[3]);
                }
            }

            if (split.size() > 4) {
                if (split[4].find(':') != std::string::npos) {
                    auto splitRGB = splitString(split[4], ':');
                    if (splitRGB.size() >= 3) {
                        p.RGB[0] = std::stoi(splitRGB[0]);
                        p.RGB[1] = std::stoi(splitRGB[1]);
                        p.RGB[2] = std::stoi(splitRGB[2]);
                    }
                } else {
                    p.FPS = std::stof(split[4]);
                }
            }

            if (split.size() > 5) {
                if (split[5].find(':') != std::string::npos) {
                    auto splitRGB = splitString(split[5], ':');
                    if (splitRGB.size() >= 3) {
                        p.RGB[0] = std::stoi(splitRGB[0]);
                        p.RGB[1] = std::stoi(splitRGB[1]);
                        p.RGB[2] = std::stoi(splitRGB[2]);
                    }
                }
            }

            m_positionValues[key].push_back(std::move(p));
            m_rectValues[key].push_back(std::move(r));
        }
    }

    // Parse legacy rects
    for (auto const &[key, value] : ini["rects"]) {
        auto rows = splitString(value, '|');
        for (auto &value2 : rows) {
            auto split = splitString(value2, ',');

            RectInfo e = {};
            e.X = std::stoi(split[0]);
            e.Y = std::stoi(split[1]);
            e.Width = std::stoi(split[2]);
            e.Height = std::stoi(split[3]);

            m_rectValues[key].push_back(std::move(e));
        }
    }
}

SkinConfig::~SkinConfig()
{
    m_positionValues.clear();
    m_numericValues.clear();
}

std::vector<PositionValue> &SkinConfig::GetPosition(std::string key)
{
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);

    if (m_positionValues.find(key) == m_positionValues.end()) {
        throw std::runtime_error("Position key not found: " + key);
    }

    return m_positionValues[key];
}

std::vector<RectInfo> &SkinConfig::GetRect(std::string key)
{
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);

    if (m_rectValues.find(key) == m_rectValues.end()) {
        throw std::runtime_error("Rect key not found: " + key);
    }

    return m_rectValues[key];
}

NoteValue &SkinConfig::GetNote(std::string key)
{
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);

    if (m_noteValues.find(key) == m_noteValues.end()) {
        throw std::runtime_error("Sprite key not found: " + key);
    }

    return m_noteValues[key];
}

std::vector<NumericValue> &SkinConfig::GetNumeric(std::string key)
{
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);

    if (m_numericValues.find(key) == m_numericValues.end()) {
        throw std::runtime_error("Numeric key not found: " + key);
    }

    return m_numericValues[key];
}

SpriteValue &SkinConfig::GetSprite(std::string key)
{
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);

    if (m_spriteValues.find(key) == m_spriteValues.end()) {
        throw std::runtime_error("Sprite key not found: " + key);
    }

    return m_spriteValues[key];
}
