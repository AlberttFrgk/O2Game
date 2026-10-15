#include "Autoplay.h"
#include "../EnvironmentSetup.hpp"
#include <algorithm>

namespace {
const NoteInfo* GetNextHitObject(const std::vector<NoteInfo>& hitObject, size_t index)
{
    int lane = hitObject[index].LaneIndex;

    for (size_t i = index + 1; i < hitObject.size(); i++) {
        if (hitObject[i].LaneIndex == lane) {
            return &hitObject[i];
        }
    }

    return nullptr;
}

double CalculateReleaseTime(const NoteInfo* currentHitObject, const NoteInfo* nextHitObject, double speed)
{
    if (currentHitObject->Type == NoteType::HOLD) {
        return currentHitObject->EndTime;
    }

    // Normal note: default hold duration is 60ms, adaptive early release if next note in same lane arrives sooner
    double holdTime = 60.0;
    if (nextHitObject != nullptr) {
        double realGap = (nextHitObject->StartTime - currentHitObject->StartTime) / (speed > 0.0 ? speed : 1.0);
        if (realGap > 1.0) {
            holdTime = std::clamp(realGap * 0.75, 30.0, 60.0);
        }
    }

    return currentHitObject->StartTime + (holdTime * (speed > 0.0 ? speed : 1.0));
}
} // namespace

std::vector<Autoplay::ReplayHitInfo> Autoplay::CreateReplay(Chart* chart)
{
    std::vector<Autoplay::ReplayHitInfo> result;
    if (!chart) return result;

    double speed = 1.0;
    if (EnvironmentSetup::Get("SongRate").size() > 0) {
        try {
            speed = std::clamp(std::stod(EnvironmentSetup::Get("SongRate")), 0.5, 2.0);
        } catch (...) {
            speed = 1.0;
        }
    }

    for (size_t i = 0; i < chart->m_notes.size(); i++) {
        auto& currentHitObject = chart->m_notes[i];
        auto nextHitObject = GetNextHitObject(chart->m_notes, i);

        double hitTime = currentHitObject.StartTime;
        double releaseTime = CalculateReleaseTime(&currentHitObject, nextHitObject, speed);

        result.push_back({ hitTime, static_cast<int>(currentHitObject.LaneIndex), ReplayHitType::KEY_DOWN });
        result.push_back({ releaseTime, static_cast<int>(currentHitObject.LaneIndex), ReplayHitType::KEY_UP });
    }

    return result;
}