#include "Rendering/Threading/GameThread.h"

GameThread::GameThread()
{
    m_run = false;
    m_background = false;
}

GameThread::~GameThread()
{
    if (m_run) {
        Stop();
    }
}

void GameThread::Run(std::function<void()> callback, bool background)
{
    m_main_cb = callback;
    m_run = true;
    m_background = background;

    if (background) {
        m_thread = std::thread([this] {
            while (m_run) {
                if (m_main_cb) {
                    m_main_cb();
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }

                std::vector<std::function<void()>> local_queue;
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    if (!m_queue_cb.empty()) {
                        std::swap(local_queue, m_queue_cb);
                    }
                }

                for (auto& cb : local_queue) {
                    if (cb) cb();
                }
            }
        });
    }
}

void GameThread::Update()
{
    if (!m_background) {
        if (m_main_cb) {
            m_main_cb();
        }

        std::vector<std::function<void()>> local_queue;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_queue_cb.empty()) {
                std::swap(local_queue, m_queue_cb);
            }
        }

        for (auto& cb : local_queue) {
            if (cb) cb();
        }
    }
}

void GameThread::QueueAction(std::function<void()> callback)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_queue_cb.push_back(callback);
}

void GameThread::Stop()
{
    if (m_background) {
        m_run = false;
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }
}

