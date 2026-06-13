#include "VideoPlayer.hpp"
#include <iostream>
#include <chrono>
#include "Rendering/Window.h"

VideoPlayer::VideoPlayer() {
}

VideoPlayer::~VideoPlayer() {
    Stop();
}

bool VideoPlayer::Load(const std::string& path) {
    Stop();
    m_path = path;

    m_formatCtx = avformat_alloc_context();
    if (avformat_open_input(&m_formatCtx, m_path.c_str(), nullptr, nullptr) != 0) {
        return false;
    }

    if (avformat_find_stream_info(m_formatCtx, nullptr) < 0) {
        avformat_close_input(&m_formatCtx);
        return false;
    }

    m_videoStreamIndex = -1;
    const AVCodec* codec = nullptr;
    
    for (unsigned int i = 0; i < m_formatCtx->nb_streams; i++) {
        if (m_formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            m_videoStreamIndex = i;
            codec = avcodec_find_decoder(m_formatCtx->streams[i]->codecpar->codec_id);
            break;
        }
    }

    if (m_videoStreamIndex == -1 || !codec) {
        avformat_close_input(&m_formatCtx);
        return false;
    }

    m_codecCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(m_codecCtx, m_formatCtx->streams[m_videoStreamIndex]->codecpar);

    m_codecCtx->thread_count = 0;
    m_codecCtx->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;

    if (avcodec_open2(m_codecCtx, codec, nullptr) < 0) {
        avcodec_free_context(&m_codecCtx);
        avformat_close_input(&m_formatCtx);
        return false;
    }

    m_width = m_codecCtx->width;
    m_height = m_codecCtx->height;

    m_frame = av_frame_alloc();
    m_frameRGB = av_frame_alloc();
    m_packet = av_packet_alloc();

    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGBA, m_width, m_height, 32);
    m_buffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));

    av_image_fill_arrays(m_frameRGB->data, m_frameRGB->linesize, m_buffer, AV_PIX_FMT_RGBA, m_width, m_height, 32);

    m_swsCtx = sws_getContext(m_width, m_height, m_codecCtx->pix_fmt,
                              m_width, m_height, AV_PIX_FMT_RGBA,
                              SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);

    m_texture = std::make_unique<Texture2D>(m_width, m_height);

    m_valid = true;
    return true;
}

void VideoPlayer::Play() {
    if (!m_valid) return;
    
    m_quit = false;
    m_playing = true;
    m_decodeThread = std::thread(&VideoPlayer::DecodeThread, this);
}

void VideoPlayer::Stop() {
    m_quit = true;
    if (m_decodeThread.joinable()) {
        m_decodeThread.join();
    }
    
    m_playing = false;
    m_valid = false;

    if (m_buffer) { av_free(m_buffer); m_buffer = nullptr; }
    if (m_frameRGB) { av_frame_free(&m_frameRGB); m_frameRGB = nullptr; }
    if (m_frame) { av_frame_free(&m_frame); m_frame = nullptr; }
    if (m_packet) { av_packet_free(&m_packet); m_packet = nullptr; }
    if (m_swsCtx) { sws_freeContext(m_swsCtx); m_swsCtx = nullptr; }
    if (m_codecCtx) { avcodec_free_context(&m_codecCtx); m_codecCtx = nullptr; }
    if (m_formatCtx) { avformat_close_input(&m_formatCtx); m_formatCtx = nullptr; }
    
    m_texture.reset();
}

void VideoPlayer::DecodeThread() {
    while (!m_quit) {
        if (!m_playing) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
        // Wait if video is too far ahead of audio
        if (m_videoTime > m_currentAudioTime + 50.0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        if (av_read_frame(m_formatCtx, m_packet) >= 0) {
            if (m_packet->stream_index == m_videoStreamIndex) {
                if (avcodec_send_packet(m_codecCtx, m_packet) == 0) {
                    while (avcodec_receive_frame(m_codecCtx, m_frame) == 0) {
                        
                        double pts = 0;
                        if (m_frame->pts != AV_NOPTS_VALUE) {
                            pts = m_frame->pts;
                        }
                        
                        double timeBase = av_q2d(m_formatCtx->streams[m_videoStreamIndex]->time_base);
                        double frameTimeMs = pts * timeBase * 1000.0;
                        
                        // Drop frame if video are way behind
                        if (frameTimeMs < m_currentAudioTime - 50.0) {
                            continue;
                        }

                        // Convert to RGBA
                        sws_scale(m_swsCtx, (uint8_t const* const*)m_frame->data,
                                  m_frame->linesize, 0, m_height,
                                  m_frameRGB->data, m_frameRGB->linesize);

                        m_videoTime = frameTimeMs;

                        // Lock and copy to shared buffer
                        {
                            std::lock_guard<std::mutex> lock(m_frameMutex);
                            int pitch = m_frameRGB->linesize[0];
                            size_t bufSize = (size_t)pitch * m_height;
                            if (m_pixelBuffer.size() != bufSize) {
                                m_pixelBuffer.resize(bufSize);
                            }
                            memcpy(m_pixelBuffer.data(), m_frameRGB->data[0], bufSize);
                            m_pitch = pitch;
                            m_frameReady = true;
                        }
                    }
                }
            }
            av_packet_unref(m_packet);
        } else {
            // EOF or error
            m_playing = false;
        }
    }
}

void VideoPlayer::Update(double currentAudioTime) {
    if (!m_playing) return;
    m_currentAudioTime = currentAudioTime;

    std::lock_guard<std::mutex> lock(m_frameMutex);
    if (m_frameReady && m_texture && m_pixelBuffer.size() > 0) {
        m_texture->UpdateTexture(m_pixelBuffer.data(), m_width, m_height, m_pitch);
        m_frameReady = false;
    }
}

void VideoPlayer::Render() {
    if (!m_valid || !m_texture) return;
    
    // Draw the texture covering the screen
    m_texture->Size = UDim2::fromOffset(GameWindow::GetInstance()->GetBufferWidth(), GameWindow::GetInstance()->GetBufferHeight());
    m_texture->Position = UDim2::fromOffset(0, 0);
    m_texture->Draw();
}
