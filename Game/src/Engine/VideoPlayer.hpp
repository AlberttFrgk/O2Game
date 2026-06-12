#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <memory>
#include "Texture/Texture2D.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

class VideoPlayer {
public:
    VideoPlayer();
    ~VideoPlayer();

    bool Load(const std::string& path);
    void Play();
    void Stop();
    
    // Call this every frame with the current audio time to sync the video
    void Update(double currentAudioTime);
    void Render();

    bool IsValid() const { return m_valid; }

private:
    void DecodeThread();

    bool m_valid = false;
    std::string m_path;

    AVFormatContext* m_formatCtx = nullptr;
    AVCodecContext* m_codecCtx = nullptr;
    int m_videoStreamIndex = -1;
    SwsContext* m_swsCtx = nullptr;

    AVFrame* m_frame = nullptr;
    AVFrame* m_frameRGB = nullptr;
    AVPacket* m_packet = nullptr;

    uint8_t* m_buffer = nullptr;

    std::atomic<bool> m_playing{false};
    std::atomic<bool> m_quit{false};
    std::thread m_decodeThread;
    std::mutex m_frameMutex;

    std::unique_ptr<Texture2D> m_texture;
    
    double m_currentAudioTime = 0.0;
    std::atomic<double> m_videoTime{0.0};
    
    int m_width = 0;
    int m_height = 0;
    
    bool m_frameReady = false;
    std::vector<uint8_t> m_pixelBuffer;
    int m_pitch = 0;
};
