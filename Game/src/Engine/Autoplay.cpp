#include "Autoplay.h"
#include "./Timing/TimingBase.h"

constexpr double kReleaseDelay = 33.34;
const double kNoteCoolHitRatio = 6.0;

NoteInfo* GetNextHitObject(std::vector<NoteInfo>& hitObject, int index)
{
    int lane = hitObject[index].LaneIndex;

    for (int i = index + 1; i < hitObject.size(); i++) {
        if (hitObject[i].LaneIndex == lane) {
            return &hitObject[i];
        }
    }

    return nullptr;
}

double CalculateReleaseTime(NoteInfo* currentHitObject, NoteInfo* nextHitObject)
{
    if (currentHitObject->Type == NoteType::HOLD) {
        return currentHitObject->EndTime;
    }

    double Time = currentHitObject->Type == NoteType::HOLD ? currentHitObject->EndTime
        : currentHitObject->StartTime;

    bool canDelayFully = nextHitObject == nullptr ||
        nextHitObject->StartTime > Time + kReleaseDelay;

    return Time + (canDelayFully ? kReleaseDelay : (nextHitObject->StartTime - Time) * 0.9);
}

std::vector<Autoplay::ReplayHitInfo> Autoplay::CreateReplay(Chart* chart)
{
    std::vector<Autoplay::ReplayHitInfo> result;

    TimingBase timingBase(chart->m_bpms, chart->m_svs, chart->InitialSvMultiplier);

    for (int i = 0; i < chart->m_notes.size(); i++) {
        auto& currentHitObject = chart->m_notes[i];
        auto nextHitObject = GetNextHitObject(chart->m_notes, i);

        double HitTime = currentHitObject.StartTime;
        double ReleaseTime = CalculateReleaseTime(&currentHitObject, nextHitObject);

        double bpmAtHit = timingBase.GetBPMAt(HitTime);
        double bpmAtRelease = timingBase.GetBPMAt(ReleaseTime);

        double beat = 60000.0 / bpmAtHit;
        double coolWindow = beat * kNoteCoolHitRatio;
        double hitError = currentHitObject.StartTime - HitTime;

        while (std::abs(hitError) > coolWindow) {
            HitTime += hitError > 0 ? 1 : -1;
            hitError = currentHitObject.StartTime - HitTime;
        }

        beat = 60000.0 / bpmAtRelease;
        coolWindow = beat * kNoteCoolHitRatio;
        double releaseError = (currentHitObject.Type == NoteType::HOLD ? currentHitObject.EndTime : currentHitObject.StartTime) - ReleaseTime;

        while (std::abs(releaseError) > coolWindow) {
            ReleaseTime += releaseError > 0 ? 1 : -1;
            releaseError = (currentHitObject.Type == NoteType::HOLD ? currentHitObject.EndTime : currentHitObject.StartTime) - ReleaseTime;
        }

        result.push_back({ HitTime, static_cast<int>(currentHitObject.LaneIndex), ReplayHitType::KEY_DOWN });
        result.push_back({ ReleaseTime, static_cast<int>(currentHitObject.LaneIndex), ReplayHitType::KEY_UP });
    }

    return result;
}