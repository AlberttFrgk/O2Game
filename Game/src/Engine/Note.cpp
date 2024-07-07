#include "Note.hpp"
#include <vector>

#include "../EnvironmentSetup.hpp"
#include "DrawableNote.hpp"
#include "GameAudioSampleCache.hpp"
#include "NoteImageCacheManager.hpp"
#include "RhythmEngine.hpp"

#define REMOVE_TIME 800
#define HOLD_COMBO_TICK 100

namespace {
    double CalculateNotePosition(double offset, double initialTrackPos, double hitPosition, double noteSpeed, bool upscroll)
    {
        return hitPosition + ((initialTrackPos - offset) * (upscroll ? noteSpeed : -noteSpeed) / 100);
    }

    double lerp(double min, double max, float alpha)
    {
        return min * (1.0 - alpha) + (max * alpha);
    }

    bool isWithinRange(double point, double minRange, double maxRange)
    {
        return (point >= minRange && point <= maxRange);
    }

    bool isCollision(double top, double bottom, double min, double max)
    {
        // Check if the top value of the rectangle is within the range
        if (top >= min && top <= max) {
            return true; // Collision detected
        }

        // Check if the bottom value of the rectangle is within the range
        if (bottom >= min && bottom <= max) {
            return true; // Collision detected
        }

        // Check if the range is completely inside the rectangle
        if (top <= min && bottom >= max) {
            return true; // Collision detected
        }

        // No collision detected
        return false;
    }

    const int length_multiplier[4] = {
        0,
        2,
        4,
        6
    };
} // namespace

Note::Note(RhythmEngine *engine, GameTrack *track)
{
    m_engine = engine;
    m_track = track;

    m_imageType = NoteImageType::LANE_1;
    m_imageBodyType = NoteImageType::HOLD_LANE_1;

    m_head = nullptr;
    m_tail = nullptr;
    m_body = nullptr;

    m_startTime = 0;
    m_endTime = 0;
    m_startBPM = 0;
    m_endBPM = 0;

    m_type = NoteType::NORMAL;
    m_lane = 0;
    m_initialTrackPosition = 0;
    m_endTrackPosition = 0;

    m_keysoundIndex = -1;
    m_state = NoteState::NORMAL_NOTE;

    m_drawAble = false;
    m_laneOffset = 0;
    m_removeAble = false;

    m_didHitHead = false;
    m_didHitTail = false;
    m_shouldDrawHoldEffect = true;

    m_hitPos = 0;
    m_relPos = 0;

    m_keyVolume = 100;
    m_keyPan = 0;

    m_lastScoreTime = -1;
}

Note::~Note()
{
    Release();
}

void Note::Load(NoteInfoDesc *desc)
{
    m_imageType = desc->ImageType;
    m_imageBodyType = desc->ImageBodyType;

    m_head = NoteImageCacheManager::GetInstance()->Depool(m_imageType);
    if (desc->Type == NoteType::HOLD) {
        m_tail = NoteImageCacheManager::GetInstance()->Depool(m_imageType);
        m_body = NoteImageCacheManager::GetInstance()->DepoolHold(m_imageBodyType);
        m_body->AnchorPoint = { 0, 0.5 };

        m_startBPM = desc->StartBPM;
        m_endBPM = desc->EndBPM;
        m_state = NoteState::HOLD_PRE;
    } else {
        m_tail = nullptr;
        m_body = nullptr;

        m_startBPM = desc->StartBPM;
        m_endBPM = 0;
        m_state = NoteState::NORMAL_NOTE;
    }

    m_trail_up = NoteImageCacheManager::GetInstance()->DepoolTrail(NoteImageType::TRAIL_UP);
    m_trail_down = NoteImageCacheManager::GetInstance()->DepoolTrail(NoteImageType::TRAIL_DOWN);

    m_startTime = desc->StartTime;
    m_endTime = desc->EndTime;
    m_type = desc->Type;
    m_lane = desc->Lane;
    m_initialTrackPosition = desc->InitialTrackPosition;
    m_endTrackPosition = desc->EndTrackPosition;
    m_keysoundIndex = desc->KeysoundIndex;

    m_keyVolume = desc->Volume;
    m_keyPan = desc->Pan;

    m_laneOffset = 0;
    m_drawAble = false;
    m_removeAble = false;
    m_ignore = true;

    m_didHitHead = false;
    m_didHitTail = false;
    m_shouldDrawHoldEffect = true;

    m_hitPos = 0;
    m_relPos = 0;

    m_lastScoreTime = -1;
}

void Note::Update(double delta)
{
    if (IsRemoveable())
        return;

    JudgeBase *judge = m_engine->GetJudge();
    double     audioPos = m_engine->GetGameAudioPosition();
    m_hitTime = m_startTime - audioPos;

    if (m_type == NoteType::NORMAL) {
        if (judge->IsMissed(this)) {
            if (!IsPassed()) {
                m_hitPos = m_startTime + m_hitTime;
                OnHit(NoteResult::MISS);
            }

            m_state = NoteState::DO_REMOVE;
        }
    } else {
        if (m_state == NoteState::HOLD_PRE) {
            if (judge->IsMissed(this)) {
                m_state = NoteState::HOLD_MISSED_ACTIVE;

                m_hitPos = m_startTime + m_hitTime;
                OnHit(NoteResult::MISS);
            }
        } else if (
            m_state == NoteState::HOLD_ON_HOLDING ||
            m_state == NoteState::HOLD_MISSED_ACTIVE ||
            m_state == NoteState::HOLD_PASSED) {

            if (m_state == NoteState::HOLD_ON_HOLDING) {
                if (m_lastScoreTime != -1 && audioPos <= m_endTime && audioPos > m_startTime) {
                    if (audioPos - m_lastScoreTime > HOLD_COMBO_TICK) {
                        m_lastScoreTime += HOLD_COMBO_TICK;
                        m_track->HandleHoldScore(HoldResult::HoldAdd);
                    }
                }
            }

            if (judge->IsMissed(this)) {
                if (m_state == NoteState::HOLD_ON_HOLDING || m_state == NoteState::HOLD_MISSED_ACTIVE) {
                    m_hitPos = m_endTime + (m_endTime - audioPos);
                    if (m_state == NoteState::HOLD_ON_HOLDING) {
                        OnRelease(NoteResult::MISS);
                    }
                }

                m_state = NoteState::DO_REMOVE;
            }
        }
    }
}

void Note::Render(double delta)
{
    if (IsRemoveable()) {
        return;
    }
    if (!m_drawAble) {
        return;
    }

    auto resolution = m_engine->GetResolution();
    auto hitPos = m_engine->GetHitPosition();
    double trackPosition = m_engine->GetTrackPosition();

    int min = -100, max = hitPos + 25;
    auto playRect = m_engine->GetPlayRectangle();

    int guideLineIndex = m_engine->GetGuideLineIndex();
    int guideLineLength = 24 * length_multiplier[guideLineIndex];

    double y1 = CalculateNotePosition(trackPosition, m_initialTrackPosition, 1000.0, m_engine->GetNotespeed(), false) / 1000.0;
    double headPosY = lerp(0.0, static_cast<double>(hitPos), static_cast<float>(y1));
    bool isHeadVisible = isWithinRange(headPosY, min, max);

    if (m_type == NoteType::HOLD) {
        double y2 = CalculateNotePosition(trackPosition, m_endTrackPosition, 1000.0, m_engine->GetNotespeed(), false) / 1000.0;
        double tailPosY = lerp(0.0, static_cast<double>(hitPos), static_cast<float>(y2));
        bool isTailVisible = isWithinRange(tailPosY, min, max);

        //if (EnvironmentSetup::GetInt("Percy") == 0) {
        //    tailPosY += m_tail->AbsoluteSize.Y;
        //}

        double bodyPosY = (headPosY + tailPosY) / 2.0;
        double bodyHeight = std::abs(headPosY - (m_head->AbsoluteSize.Y / 2.0)) - (tailPosY - (m_head->AbsoluteSize.Y / 2.0));

        m_body->Position = UDim2::fromOffset(m_laneOffset, bodyPosY - (m_head->AbsoluteSize.Y / 2.0));
        m_body->Size = { 1, 0, 0, bodyHeight };

        float transparency = 0.9f;
        if (m_hitResult >= NoteResult::GOOD && m_state == NoteState::HOLD_ON_HOLDING) {
            transparency = 1.0f;
        }
        else if (m_hitResult >= NoteResult::MISS && m_state == NoteState::HOLD_MISSED_ACTIVE) {
            transparency = 0.7f;
        }

        m_body->TintColor = { transparency, transparency, transparency };
        m_body->SetIndexAt(m_engine->GetNoteImageIndex());
        m_body->Draw(delta, &playRect);

        if (isTailVisible) {
            m_tail->Position = UDim2::fromOffset(m_laneOffset, tailPosY);
            m_tail->SetIndexAt(m_engine->GetNoteImageIndex());
            m_tail->Draw(delta, &playRect);

            if (guideLineLength > 0) {
                m_trail_down->Position = m_tail->Position;
                m_trail_down->Size = UDim2::fromOffset(1, guideLineLength);
                m_trail_down->AnchorPoint = { 0, 0 };
                m_trail_down->AlphaBlend = true;
                m_trail_down->Draw(delta, &playRect);

                m_trail_down->Position = m_tail->Position + UDim2::fromOffset(m_tail->AbsoluteSize.X, 0);
                m_trail_down->AnchorPoint = { 1, 0 };
                m_trail_down->AlphaBlend = true;
                m_trail_down->Draw(delta, &playRect);

                m_trail_up->Position = m_tail->Position + UDim2::fromOffset(0, -m_tail->AbsoluteSize.Y);
                m_trail_up->Size = UDim2::fromOffset(1, guideLineLength);
                m_trail_up->AnchorPoint = { 0, 1 };
                m_trail_up->AlphaBlend = true;
                m_trail_up->Draw(delta, &playRect);

                m_trail_up->Position = m_tail->Position + UDim2::fromOffset(m_tail->AbsoluteSize.X, -m_tail->AbsoluteSize.Y);
                m_trail_up->AnchorPoint = { 1, 1 };
                m_trail_up->AlphaBlend = true;
                m_trail_up->Draw(delta, &playRect);
            }
        }
    }

    if (isHeadVisible) {
        m_head->Position = UDim2::fromOffset(m_laneOffset, headPosY);
        m_head->SetIndexAt(m_engine->GetNoteImageIndex());
        m_head->Draw(delta, &playRect);

        if (guideLineLength > 0) {
            m_trail_down->Position = m_head->Position;
            m_trail_down->Size = UDim2::fromOffset(1, guideLineLength);
            m_trail_down->AnchorPoint = { 0, 0 };
            m_trail_down->AlphaBlend = true;
            m_trail_down->Draw(delta, &playRect);

            m_trail_down->Position = m_head->Position + UDim2::fromOffset(m_head->AbsoluteSize.X, 0);
            m_trail_down->AnchorPoint = { 1, 0 };
            m_trail_down->AlphaBlend = true;
            m_trail_down->Draw(delta, &playRect);

            m_trail_up->Position = m_head->Position + UDim2::fromOffset(0, -m_head->AbsoluteSize.Y);
            m_trail_up->Size = UDim2::fromOffset(1, guideLineLength);
            m_trail_up->AnchorPoint = { 0, 1 };
            m_trail_up->AlphaBlend = true;
            m_trail_up->Draw(delta, &playRect);

            m_trail_up->Position = m_head->Position + UDim2::fromOffset(m_head->AbsoluteSize.X, -m_head->AbsoluteSize.Y);
            m_trail_up->AnchorPoint = { 1, 1 };
            m_trail_up->AlphaBlend = true;
            m_trail_up->Draw(delta, &playRect);
        }
    }
}

double Note::GetInitialTrackPosition() const
{
    return m_initialTrackPosition;
}

double Note::GetStartTime() const
{
    return m_startTime;
}

double Note::GetBPMTime() const
{
    if (GetType() == NoteType::HOLD) {
        if (m_state == NoteState::HOLD_PRE) {
            return m_startBPM;
        } else {
            return m_endBPM;
        }
    } else {
        return m_startBPM;
    }
}

double Note::GetHitTime() const
{
    if (GetType() == NoteType::HOLD) {
        if (m_state == NoteState::HOLD_PRE) {
            return m_startTime;
        } else {
            return m_endTime;
        }
    } else {
        return m_startTime;
    }
}

int Note::GetKeysoundId() const
{
    return m_keysoundIndex;
}

int Note::GetKeyVolume() const
{
    return m_keyVolume;
}

int Note::GetKeyPan() const
{
    return m_keyPan;
}

NoteType Note::GetType() const
{
    return m_type;
}

std::tuple<bool, NoteResult> Note::CheckHit()
{
    JudgeBase *judge = m_engine->GetJudge();

    if (m_type == NoteType::NORMAL) {
        double time_to_end = m_engine->GetGameAudioPosition() - m_startTime;
        auto result = judge->CalculateResult(this);
        if (std::get<bool>(result)) {
            m_ignore = false;
        }
        else if (time_to_end) {
            m_ignore = true;
        }

        return result;
    }
    else {
        if (m_state == NoteState::HOLD_PRE) {
            double time_to_end = m_engine->GetGameAudioPosition() - m_startTime;
            auto result = judge->CalculateResult(this);
            if (std::get<bool>(result)) {
                m_ignore = false;
            }
            else if (time_to_end) {
                m_ignore = true;
            }

            return result;
        }
        else if (m_state == NoteState::HOLD_MISSED_ACTIVE) {
            double time_to_end = m_engine->GetGameAudioPosition() - m_endTime;
            auto result = judge->CalculateResult(this);
            if (std::get<bool>(result)) {
                m_ignore = false;
            }
            else if (time_to_end) {
                m_ignore = true;
            }

            return result;
        }

        return { false, NoteResult::MISS };
    }
}

std::tuple<bool, NoteResult> Note::CheckRelease()
{
    if (m_type == NoteType::HOLD) {
        double time_to_end = m_engine->GetGameAudioPosition() - m_endTime;
        JudgeBase *judge = m_engine->GetJudge();

        if (m_state == NoteState::HOLD_ON_HOLDING || m_state == NoteState::HOLD_MISSED_ACTIVE) {
            auto result = judge->CalculateResult(this);

            if (std::get<bool>(result)) {
                if (m_state == NoteState::HOLD_MISSED_ACTIVE) {
                    m_ignore = false;
                    return { true, NoteResult::BAD };
                }

                return result;
            }

            if (m_state == NoteState::HOLD_ON_HOLDING) {
                if (time_to_end) {
                    m_ignore = true;
                }
                return { true, NoteResult::MISS };
            }
            else {
                if (time_to_end) {
                    m_ignore = true;
                }
                return { false, NoteResult::MISS };
            }
        }
    }

    return { false, NoteResult::MISS };
}

void Note::OnHit(NoteResult result)
{
    if (m_type == NoteType::HOLD) {
        if (m_state == NoteState::HOLD_PRE) {
            m_didHitHead = true;
            m_state = NoteState::HOLD_ON_HOLDING;
            m_lastScoreTime = m_engine->GetGameAudioPosition();

            m_hitResult = result;
            m_track->HandleHoldScore(HoldResult::HoldAdd);
            m_track->HandleScore({ result,
                                   m_hitPos,
                                   false,
                                   m_ignore,
                                   2 });
        } else if (m_state == NoteState::HOLD_MISSED_ACTIVE) {
            m_didHitHead = true;
            m_state = NoteState::HOLD_PASSED;

            m_track->HandleHoldScore(HoldResult::HoldBreak);
            m_track->HandleScore({ result,
                                   m_hitPos,
                                   true,
                                   m_ignore,
                                   2 });
        }
    } else {
        m_state = NoteState::NORMAL_NOTE_PASSED;
        m_track->HandleScore({ result,
                               m_hitPos,
                               false,
                               m_ignore,
                               1 });
    }
}

void Note::OnRelease(NoteResult result)
{
    if (m_type == NoteType::HOLD) {
        if (m_state == NoteState::HOLD_ON_HOLDING || m_state == NoteState::HOLD_MISSED_ACTIVE) {
            m_lastScoreTime = -1;

            if (result == NoteResult::MISS) {
                //GameAudioSampleCache::Stop(m_keysoundIndex);
                m_state = NoteState::HOLD_MISSED_ACTIVE;

                m_track->HandleHoldScore(HoldResult::HoldBreak);
                m_track->HandleScore({ result,
                                       m_hitPos,
                                       true,
                                       m_ignore,
                                       2 });
            } else {
                m_state = NoteState::HOLD_PASSED;
                m_didHitTail = true;
                m_track->HandleScore({ result,
                                       m_hitPos,
                                       true,
                                       m_ignore,
                                       2 });
            }
        }
    }
}

void Note::SetXPosition(int x)
{
    m_laneOffset = x;
}

void Note::SetDrawable(bool drawable)
{
    m_drawAble = drawable;
}

bool Note::IsHoldEffectDrawable()
{
    return m_shouldDrawHoldEffect;
}

bool Note::IsDrawable()
{
    if (m_removeAble)
        return false;

    return m_drawAble;
}

bool Note::IsRemoveable()
{
    return m_state == NoteState::DO_REMOVE;
}

bool Note::IsPassed()
{
    return m_state == NoteState::NORMAL_NOTE_PASSED || m_state == NoteState::HOLD_PASSED || IsRemoveable();
}

bool Note::IsHeadHit()
{
    return m_didHitHead;
}

bool Note::IsTailHit()
{
    return m_didHitTail;
}

void Note::Release()
{
    m_state = NoteState::DO_REMOVE;
    m_removeAble = true;

    auto cacheManager = NoteImageCacheManager::GetInstance();

    cacheManager->RepoolTrail(m_trail_down, NoteImageType::TRAIL_DOWN);
    cacheManager->RepoolTrail(m_trail_up, NoteImageType::TRAIL_UP);

    m_trail_up = nullptr;
    m_trail_down = nullptr;

    if (m_type == NoteType::HOLD) {
        cacheManager->Repool(m_head, m_imageType);
        m_head = nullptr;

        cacheManager->Repool(m_tail, m_imageType);
        m_tail = nullptr;

        cacheManager->RepoolHold(m_body, m_imageBodyType);
        m_body = nullptr;
    } else {
        cacheManager->Repool(m_head, m_imageType);
        m_head = nullptr;
    }
}