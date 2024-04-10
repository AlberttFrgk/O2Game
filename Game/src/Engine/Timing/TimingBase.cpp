#include "TimingBase.h"

TimingBase::TimingBase(std::vector<TimingInfo> &_timings, std::vector<TimingInfo> &_velocities, double base)
{
    timings = _timings;
    velocities = _velocities;
    base_multiplier = base;
}

static TimingInfo& FindTimingAt(std::vector<TimingInfo>& timings, double offset)
{
    int min = 0, max = (int)timings.size() - 1;
    int left = min, right = max;

    while (left <= right) {
        int mid = (left + right) / 2;

        bool afterMid = mid < 0 || timings[mid].StartTime < offset;
        bool beforeMid = static_cast<unsigned long long>(mid) + 1 >= timings.size() || offset < timings[static_cast<std::vector<TimingInfo, std::allocator<TimingInfo>>::size_type>(mid) + 1].StartTime;

        if (afterMid && beforeMid) {
            return timings[mid];
        }
        else if (afterMid) {
            left = mid + 1;
        }
        else {
            right = mid - 1;
        }
    }

    return timings[0];
}

double TimingBase::GetBeatAt(double offset)
{
    return FindTimingAt(timings, offset).CalculateBeat(offset);
}

double TimingBase::GetBPMAt(double offset)
{
    double BPM = FindTimingAt(timings, offset).Value;

    // Handle BPM overflow at int32_t range
    if (BPM >= INT_MAX || BPM <= -INT_MAX) {
        BPM = fmod(BPM, INT_MAX);
    }

    return BPM;
}

double TimingBase::GetOffsetAt(double offset)
{
    return GetOffsetAt(offset, 0);
}

double TimingBase::GetOffsetAt(double offset, int index)
{
    return 0;
}
