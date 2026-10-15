#include <atomic>
#include <cmath>
#include <iostream>


#include "Audio/AudioManager.h"
#include "Configuration.h"
#include "Data/Imgui/imgui_impl_sdl2.h"
#include "Data/Imgui/imgui_impl_sdlrenderer2.h"
#include "Fonts/FontResources.h"
#include "Game.h"
#include "Imgui/ImguiUtil.h"
#include "Logs/Console.h"
#include "MsgBox.h"
#include "Rendering/Vulkan/VulkanEngine.h"
#include "Rendering/Window.h"
#include "Texture/MathUtils.h"
#include "Texture/Text.h"
#include <Logs.h>
#include <SDL2/SDL_image.h>


constexpr auto kInputDefaultRate = 1000.0;
constexpr auto kMenuDefaultRate = 60.0;
constexpr auto kAudioDefaultRate = 60.0;

namespace {

	bool InitSDL()
	{
		if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
			return false;
		}

		return IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG) != 0;
	}

	void DeInitSDL()
	{
		SDL_Quit();
		IMG_Quit();
	}

	thread_local double curTick = 0.0;
	thread_local double lastTick = 0.0;

	double FrameLimit(double MaxFrameRate)
	{
		double newTick = static_cast<double>(SDL_GetPerformanceCounter()) * 1000.0 / SDL_GetPerformanceFrequency();
		if (curTick == 0.0) {
			curTick = newTick;
		}

		if (MaxFrameRate >= 5000.0 || MaxFrameRate <= 0.0) {
			double delta = (newTick - curTick) / 1000.0;
			lastTick = curTick;
			curTick = newTick;

			if (delta > 0.1)
				delta = 0.1;
			if (delta <= 0.0)
				delta = 0.000001;

			return delta;
		}

		const double targetFrameTime = 1000.0 / MaxFrameRate;
		double		 frameTime = newTick - curTick;

		if (frameTime < targetFrameTime) {
			double delayTime = targetFrameTime - frameTime;

			if (delayTime > 1.5) {
				std::this_thread::sleep_for(std::chrono::microseconds(static_cast<long long>((delayTime - 1.0) * 1000.0)));
			}

			while (true) {
				newTick = static_cast<double>(SDL_GetPerformanceCounter()) * 1000.0 / SDL_GetPerformanceFrequency();
				if (newTick - curTick >= targetFrameTime) {
					break;
				}
				std::this_thread::yield();
			}
		}

		double delta = (newTick - curTick) / 1000.0;

		lastTick = curTick;
		curTick = newTick;

		if (delta > 0.1)
			delta = 0.1;
		if (delta <= 0.0)
			delta = 0.000001;

		return delta;
	}
} // namespace

Game::Game()
{
	m_frameLimit = 60.0;
	m_running = false;
	m_notify = false;

	m_window = nullptr;
	m_renderer = nullptr;
	m_inputManager = nullptr;
	m_sceneManager = nullptr;

	m_renderMode = RendererMode::DIRECTX11;
	m_threadMode = ThreadMode::MULTI_THREAD;
}

Game::~Game()
{
	if (m_running) {
		Stop();

		if (m_notify) {
			std::unique_lock<std::mutex> lock(m_mutex);
			m_cv.wait(lock, [this] { return !m_notify; });
		}
	}

	SceneManager::Release();
	AudioManager::Release();
	InputManager::Release();
	Renderer::Release();
	GameWindow::Release();

	DeInitSDL();
}

bool Game::Init()
{
	Logs::Write("Game", "Initializing SDL...");
	if (!InitSDL()) {
		Logs::Write("Game", "SDL failed to initialize: %s", SDL_GetError());
		MsgBox::ShowOut("SDL Failed to Initialize", "EstEngine Error");
		return false;
	}
	Logs::Write("Game", "SDL initialized.");

	m_window = GameWindow::GetInstance();

	int width = m_windowWidth;
	int height = m_windowHeight;
	if (m_fullscreen) {
		SDL_DisplayMode dm;
		SDL_GetCurrentDisplayMode(0, &dm);
		width = dm.w;
		height = dm.h;
	}

	Logs::Write("Game", "Creating window (%dx%d)...", width, height);
	if (!m_window->Create(m_renderMode, "Game", width, height, m_bufferWidth, m_bufferHeight)) {
		Logs::Write("Game", "Failed to create game window: %s", SDL_GetError());
		return false;
	}
	Logs::Write("Game", "Game window created.");

	m_renderer = Renderer::GetInstance();
	Logs::Write("Game", "Creating renderer...");
	if (!m_renderer->Create(m_renderMode, m_window)) {
		Logs::Write("Game", "Failed to create renderer: %s", SDL_GetError());
		return false;
	}
	Logs::Write("Game", "Renderer created.");

	m_threadMode = ThreadMode::SINGLE_THREAD;

	m_inputManager = InputManager::GetInstance();

	m_sceneManager = SceneManager::GetInstance();
	m_sceneManager->SetParent(this);

	Logs::Write("Game", "Initializing Audio Manager...");
	if (!AudioManager::GetInstance()->Init(m_window)) {
		Logs::Write("Game", "Failed to initialize Audio Manager.");
		return false;
	}
	Logs::Write("Game", "Audio Manager initialized.");

	Logs::Write("Game", "Preloading font caches...");
	FontResources::PreloadFontCaches();
	m_currentFade = 0;
	m_targetFade = 0;

	Logs::Write("Game", "Game::Init completed successfully.");
	return true;
}

void Game::Run()
{
	m_running = true;
	m_notify = true;
	// m_frameLimit = m_frameLimit == 0 ? kMenuDefaultRate : m_frameLimit; // 0 is used for VSync
	m_frameLimitMode = FrameLimitMode::MENU;

	mAudioThread.Run([&] {
		double delta = FrameLimit(kAudioDefaultRate);
		AudioManager::GetInstance()->Update(delta);
	},
					 true);

	std::mutex m1, m2;

	int	   frameWithoutSwapchain = 0;
	double time = 0;

	std::atomic<double> current_fps{ 0.0 };
	std::atomic<double> current_latency{ 0.0 };
	std::atomic<int>	current_fps_int{ 0 };

	bool show_fps = Configuration::Load("Game", "ShowFPS") == "1";
	bool show_latency = Configuration::Load("Game", "ShowLatency") == "1";

	fpsText = std::make_unique<Text>(14);
	fpsText->DrawOverEverything = true;
	latencyText = std::make_unique<Text>(14);
	latencyText->DrawOverEverything = true;

	uint64_t perf_freq = SDL_GetPerformanceFrequency();
	uint64_t fps_window_start = SDL_GetPerformanceCounter();
	int		 fps_window_frames = 0;

	auto RecordRenderFrame = [&]() {
		fps_window_frames++;
		uint64_t now = SDL_GetPerformanceCounter();
		double	 elapsed = static_cast<double>(now - fps_window_start) / static_cast<double>(perf_freq);
		if (elapsed >= 0.10) {
			if (fps_window_frames > 0 && elapsed > 0.0) {
				double fps = static_cast<double>(fps_window_frames) / elapsed;
				double latency = (elapsed / static_cast<double>(fps_window_frames)) * 1000.0;
				current_fps.store(fps);
				current_latency.store(latency);
				current_fps_int.store(static_cast<int>(std::round(fps)));
			}
			fps_window_frames = 0;
			fps_window_start = now;
		}
	};

	auto DrawFPSOverlay = [&]() {
		if (!show_fps)
			return;

		char buffer[128];
		snprintf(buffer, sizeof(buffer), "%.0ffps", current_fps.load());
		fpsText->Position = UDim2::fromOffset(3, 0);
		fpsText->Draw(buffer);

		if (show_latency) {
			char latency_buffer[128];
			snprintf(latency_buffer, sizeof(latency_buffer), "%.2fms", current_latency.load());
			latencyText->Position = UDim2::fromOffset(3, 12);
			latencyText->Draw(latency_buffer);
		}
	};

	mRenderThread.Run([&] {
		if (m_threadMode == ThreadMode::MULTI_THREAD) {
			double delta = 0;

			switch (m_frameLimitMode) {
				case FrameLimitMode::GAME:
				{
					delta = FrameLimit(m_frameLimit);
					break;
				}
				case FrameLimitMode::MENU:
				{
					delta = FrameLimit(kMenuDefaultRate);
					break;
				}
			}

			CheckFont();

			Update(delta);

			UpdateFade(delta);

			if (!m_minimized && m_renderer->BeginRender()) {
				if (frameWithoutSwapchain > 0) {
					Logs::Puts("[Game] Game iterate %d times without swap chain", frameWithoutSwapchain);
					frameWithoutSwapchain = 0;
					fps_window_start = SDL_GetPerformanceCounter();
					fps_window_frames = 0;
				}

				std::lock_guard<std::mutex> lock(m1);

				Render(delta);
				DrawFPSOverlay();
				MsgBox::Draw();
				DrawFade(delta);

				DrawConsole();
				m_renderer->EndRender();

				RecordRenderFrame();
			} else {
				frameWithoutSwapchain++;

				// Flush ImGui events when skipping render so the queue doesn't overflow and freeze
				ImguiUtil::NewFrame();
				ImguiUtil::Reset();
			}
		} else {
			FrameLimit(15.0f);
		}
	},
					  true);

	m_inputManager->ListenKeyEvent([&](const KeyState &state) {
		if (state.type == KeyEventType::KEY_DOWN) {
			m_sceneManager->OnKeyDown(state);
		} else {
			m_sceneManager->OnKeyUp(state);
		}
	});

	m_inputManager->ListenMouseEvent([&](const MouseState &state) {
		if (state.isDown) {
			m_sceneManager->OnMouseDown(state);
		} else {
			m_sceneManager->OnMouseUp(state);
		}
	});

	m_minimized = false;
	mLocalThread.Run([&] {
		double delta = 0;
		switch (m_frameLimitMode) {
			case FrameLimitMode::GAME:
			{
				delta = FrameLimit(m_threadMode == ThreadMode::MULTI_THREAD ? kInputDefaultRate : m_frameLimit);
				break;
			}
			case FrameLimitMode::MENU:
			{
				delta = FrameLimit(kMenuDefaultRate);
				break;
			}
		}

		m_imguiInterval += delta;

		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
				case SDL_QUIT:
					MsgBox::Show("Quit", "Quit confirmation", "Are you sure you want to quit?", MsgBoxType::YESNO);
					break;
				case SDL_DROPFILE:
					OnDropFile(event.drop.file);
					SDL_free(event.drop.file);
					break;
			}

			m_minimized = SDL_GetWindowFlags(m_window->GetWindow()) & SDL_WINDOW_MINIMIZED;

			ImGui_ImplSDL2_ProcessEvent(&event);
			m_inputManager->Update(event);
		}

		if (MsgBox::GetResult("Quit") == 1) {
			Stop();
		}

		if (m_renderer->IsVulkan() && !m_minimized) {
			m_renderer->ReInitVulkan();
		}

		if (m_threadMode == ThreadMode::MULTI_THREAD) {
			Input(delta);
		} else {
			CheckFont();

			Input(delta);

			Update(delta);

			UpdateFade(delta);

			if (!m_minimized && m_renderer->BeginRender()) {
				if (frameWithoutSwapchain > 0) {
					Logs::Puts("[Game] Game iterate %d times without swap chain", frameWithoutSwapchain);
					frameWithoutSwapchain = 0;
					fps_window_start = SDL_GetPerformanceCounter();
					fps_window_frames = 0;
				}

				Render(delta);
				DrawFPSOverlay();
				MsgBox::Draw();
				DrawFade(delta);

				DrawConsole();
				m_renderer->EndRender();

				RecordRenderFrame();
			} else {
				frameWithoutSwapchain++;

				// Flush ImGui events when skipping render so the queue doesn't overflow and freeze
				ImguiUtil::NewFrame();
				ImguiUtil::Reset();
			}
		}

		if ((time += delta) >= 1.0) {
			char title_buf[128];
			snprintf(title_buf, sizeof(title_buf), "FPS: %d Latency: %.2f ms", current_fps_int.load(), current_latency.load());
			std::string title_str = title_buf;
			m_window->SetWindowSubTitle(title_str);

			show_fps = Configuration::Load("Game", "ShowFPS") == "1";
			show_latency = Configuration::Load("Game", "ShowLatency") == "1";

			time = 0;
		}
	},
					 false);

	while (m_running) {
		mLocalThread.Update();
	}

	mAudioThread.Stop();
	mRenderThread.Stop();

	m_notify = false;
}

void Game::DrawFade(double delta)
{
	if (static_cast<int>(m_currentFade) != 0) {
		auto  drawList = ImGui::GetForegroundDrawList();
		float a = m_currentFade / 100.0f;

		drawList->AddRectFilled(ImVec2(0, 0), MathUtil::ScaleVec2(m_window->GetBufferWidth(), m_window->GetBufferHeight()), IM_COL32(0, 0, 0, a * 255));
	}
}

void Game::DrawConsole()
{
	Console::Draw();
}

void Game::UpdateFade(double delta)
{
	if (static_cast<int>(m_currentFade) != static_cast<int>(m_targetFade)) {
		float increment = (static_cast<float>(delta) * 5.0f) * 100.0f;

		// compare it using epsilon
		if (std::abs(m_currentFade - m_targetFade) < FLT_EPSILON) {
			m_currentFade = m_targetFade;
		} else {
			if (m_currentFade < m_targetFade) {
				m_currentFade = std::clamp(m_currentFade + increment, 0.0f, 100.0f);
			} else {
				m_currentFade = std::clamp(m_currentFade - increment, 0.0f, 100.0f);
			}
		}
	}
}

void Game::CheckFont()
{
	if (m_window->ShouldResizeRenderer()) {
		m_renderer->Resize();

		FontResources::DoRebuild();
		m_window->HandleResizeRenderer();
	}

	if (FontResources::ShouldRebuild()) {
		FontResources::PreloadFontCaches();
	}
}

void Game::Stop()
{
	if (m_running) {
		{
			std::unique_lock<std::mutex> lock(m_mutex);
			m_running = false;
			m_notify = true;
		}
		m_cv.notify_one();
	}
}

void Game::SetThreadMode(ThreadMode mode)
{
	m_threadMode = mode;
}

void Game::SetRenderMode(RendererMode mode)
{
	m_renderMode = mode;
}

void Game::SetFrameLimitMode(FrameLimitMode mode)
{
	m_frameLimitMode = mode;
}

void Game::SetFramelimit(double frameRate)
{
	m_frameLimit = frameRate;
}

void Game::SetBufferSize(int width, int height)
{
	m_bufferWidth = width;
	m_bufferHeight = height;
}

void Game::SetWindowSize(int width, int height)
{
	m_windowWidth = width;
	m_windowHeight = height;
}

void Game::SetFullscreen(bool fullscreen)
{
	m_fullscreen = fullscreen;
}

GameThread *Game::GetRenderThread()
{
	return &mRenderThread;
}

GameThread *Game::GetMainThread()
{
	return &mLocalThread;
}

void Game::DisplayFade(int transparency)
{
	m_targetFade = static_cast<float>(transparency);
}

float Game::GetDisplayFade()
{
	return m_currentFade;
}

ThreadMode Game::GetThreadMode()
{
	return m_threadMode;
}

void Game::Update(double deltaTime)
{
}

void Game::Render(double deltaTime)
{
}

void Game::Input(double deltaTime)
{
}

void Game::Mouse(double deltaTime)
{
}

void Game::OnDropFile(std::string path)
{
}
