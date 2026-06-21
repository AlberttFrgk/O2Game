#pragma once
#include <functional>
#include <thread>
#include <vector>
#include <mutex>
#include <atomic>

class GameThread
{
public:
    GameThread();
    ~GameThread();

    void Run(std::function<void()> callback, bool background);
    void QueueAction(std::function<void()> callback);

    void Update();
    void Stop();

private:
    std::atomic<bool> m_run{false};
    std::atomic<bool> m_background{false};

    std::mutex m_mutex;

    std::thread m_thread;

    std::function<void()>              m_main_cb;
    std::vector<std::function<void()>> m_queue_cb;
};