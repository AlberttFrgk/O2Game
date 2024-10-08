#pragma once
#include "../Rendering/WindowsTypes.h"
#include "AudioSampleChannel.h"
#include <filesystem>
#include <iostream>

class AudioSample
{
#if _DEBUG
    const char SIGNATURE[25] = "AudioSample";
#endif

public:
    AudioSample(std::string id);
    ~AudioSample();

    bool Create(uint8_t *buffer, size_t size);
    bool Create(std::filesystem::path path);
    bool CreateFromData(int sampleFlags, int sampleRate, int sampleChannels, int sampleLength, void *sampleData);
    bool CreateSilent();
    void SetRate(double rate);

    std::string GetId() const;

    std::unique_ptr<AudioSampleChannel> CreateChannel();

private:
    void        CheckAudioTime();
    std::string m_id;

    uint32_t m_handle;

    float m_rate;
    float m_vol;

    bool m_silent;
    bool m_pitch;
};