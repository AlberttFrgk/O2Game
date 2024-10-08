#include <bass.h>
#include <bass_fx.h>
#include <vector>

#include "Audio/AudioSampleChannel.h"
#include <iostream>

AudioSampleChannel::AudioSampleChannel()
{
    m_hCurrentSample = 0;
    m_rate = 1.0f;
    m_vol = 1.0f;
    m_pan = 0.0f;
    m_pitch = false;
    m_hasPlayed = false;
    m_silent = true;
}

AudioSampleChannel::AudioSampleChannel(uint32_t sampleHandle, float rate, float vol, bool pitch)
{
    m_hCurrentSample = BASS_SampleGetChannel(sampleHandle, BASS_SAMCHAN_STREAM);
    if (!m_hCurrentSample) {
        ::printf("[BASS] Error: %d\n", BASS_ErrorGetCode());
    }

    m_rate = rate;
    m_vol = vol;
    m_pan = 0.0f;
    m_pitch = pitch;
    m_hasPlayed = false;
    m_silent = false;
}

AudioSampleChannel::~AudioSampleChannel()
{
    Stop();
}

void AudioSampleChannel::SetVolume(int vol)
{
    m_vol = static_cast<float>(vol);
}

void AudioSampleChannel::SetPan(int pan)
{
    m_pan = static_cast<float>(pan);
}

bool AudioSampleChannel::HasPlayed()
{
    return m_hasPlayed;
}

bool AudioSampleChannel::Play()
{
    if (m_silent) {
        ::printf("Playing silent audio!\n");
        m_hasPlayed = true;

        return true;
    }

    if (!m_hCurrentSample) {
        return false;
    }

    // Pitch is not possible atm, in sample channel!
    if (m_rate != 1.0f) {
        float frequency = 48000.0f;
        BASS_ChannelGetAttribute((HSTREAM)m_hCurrentSample, BASS_ATTRIB_FREQ, &frequency);
        BASS_ChannelSetAttribute((HSTREAM)m_hCurrentSample, BASS_ATTRIB_FREQ, frequency * m_rate);
    }

    float vol = m_vol / 100.0f;
    float pan = m_pan / 100.0f;

    BASS_ChannelSetAttribute((HSTREAM)m_hCurrentSample, BASS_ATTRIB_VOL, vol);
    BASS_ChannelSetAttribute((HSTREAM)m_hCurrentSample, BASS_ATTRIB_PAN, pan);

    if (!BASS_ChannelPlay((HSTREAM)m_hCurrentSample, FALSE)) {
        return false;
    }

    m_hasPlayed = true;
    return true;
}

bool AudioSampleChannel::Stop()
{
    if (m_silent) {
        return true;
    }

    if (!m_hCurrentSample) {
        return false;
    }

    BASS_ChannelStop((HSTREAM)m_hCurrentSample);
    BASS_ChannelFree((HSTREAM)m_hCurrentSample);

    return true;
}

bool AudioSampleChannel::Pause()
{
    if (m_silent) {
        return true;
    }

    if (!m_hCurrentSample) {
        return false;
    }

    if (!BASS_ChannelPause((HSTREAM)m_hCurrentSample)) {
        return false;
    }

    return true;
}

uint32_t AudioSampleChannel::GetHandle() const
{
    return m_hCurrentSample;
}
bool AudioSampleChannel::IsPlaying()
{
    return BASS_ChannelIsActive((HSTREAM)m_hCurrentSample) == BASS_ACTIVE_PLAYING;
}

bool AudioSampleChannel::IsStopped()
{
    return m_silent || BASS_ChannelIsActive((HSTREAM)m_hCurrentSample) == BASS_ACTIVE_STOPPED;
}
