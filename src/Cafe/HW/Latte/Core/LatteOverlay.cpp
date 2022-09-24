#include "Cafe/HW/Latte/Core/LatteOverlay.h"
#include "Cafe/HW/Latte/Core/LattePerformanceMonitor.h"
#include "gui/guiWrapper.h"

#include "config/CemuConfig.h"

#include "Cafe/HW/Latte/Renderer/Renderer.h"
#include "config/ActiveSettings.h"

#include <imgui.h>
#include "resource/IconsFontAwesome5.h"
#include "imgui/imgui_extension.h"

#include "input/InputManager.h"

#include <cinttypes>

#if BOOST_OS_WINDOWS
#include <Psapi.h>
#include <winternl.h>
#pragma comment(lib, "ntdll.lib")
#endif

struct OverlayStats
{
	OverlayStats() {};

	int processor_count = 1;

	// cemu cpu stats
	uint64_t last_cpu{}, kernel{}, user{};

	// global cpu stats
	struct ProcessorTime
	{
		uint64_t idle{}, kernel{}, user{};
	};

	std::vector<ProcessorTime> processor_times;

	double fps{};
	uint32 draw_calls_per_frame{};
	float cpu_usage{}; // cemu cpu usage in %
	std::vector<float> cpu_per_core; // global cpu usage in % per core
	uint32 ram_usage{}; // ram usage in MB

	int vramUsage{}, vramTotal{}; // vram usage in mb
} g_state{};

extern std::atomic_int g_compiled_shaders_total;
extern std::atomic_int g_compiled_shaders_async;

std::atomic_int g_compiling_pipelines;
std::atomic_int g_compiling_pipelines_async;
std::atomic_uint64_t g_compiling_pipelines_syncTimeSum;

extern std::mutex g_friend_notification_mutex;
extern std::vector< std::pair<std::string, int> > g_friend_notifications;

std::mutex g_notification_mutex;
std::vector< std::pair<std::string, int> > g_notifications;

void LatteOverlay_pushNotification(const std::string& text, sint32 duration)
{
	std::unique_lock lock(g_notification_mutex);
	g_notifications.emplace_back(text, duration);
}

struct OverlayList
{
	std::wstring text;
	float pos_x = 0;
	float pos_y = 0;
	float width;

	OverlayList(std::wstring text, float width)
		: text(std::move(text)), width(width) {}
};

const auto kPopupFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

const float kBackgroundAlpha = 0.65f;
void LatteOverlay_renderOverlay(ImVec2& position, ImVec2& pivot, sint32 direction)
{
	auto& config = GetConfig();
	
	const auto font = ImGui_GetFont(14.0f * (float)config.overlay.text_scale / 100.0f);
	ImGui::PushFont(font);

	const ImVec4 color = ImGui::ColorConvertU32ToFloat4(config.overlay.text_color);
	ImGui::PushStyleColor(ImGuiCol_Text, color);
	// stats overlay
	if (config.overlay.fps || config.overlay.drawcalls || config.overlay.cpu_usage || config.overlay.cpu_per_core_usage || config.overlay.ram_usage)
	{
		ImGui::SetNextWindowPos(position, ImGuiCond_Always, pivot);
		ImGui::SetNextWindowBgAlpha(kBackgroundAlpha);
		if (ImGui::Begin("Stats overlay", nullptr, kPopupFlags))
		{
			if (config.overlay.fps)
				ImGui::Text("FPS: %.2lf", g_state.fps);

			if (config.overlay.drawcalls)
				ImGui::Text("Draws/f: %d", g_state.draw_calls_per_frame);

			if (config.overlay.cpu_usage)
				ImGui::Text("CPU: %.2lf%%", g_state.cpu_usage);

			if (config.overlay.cpu_per_core_usage)
			{
				for (sint32 i = 0; i < g_state.processor_count; ++i)
				{
					ImGui::Text("CPU #%d: %.2lf%%", i + 1, g_state.cpu_per_core[i]);
				}
			}

			if (config.overlay.ram_usage)
				ImGui::Text("RAM: %dMB", g_state.ram_usage);

			if(config.overlay.vram_usage && g_state.vramUsage != -1 && g_state.vramTotal != -1)
				ImGui::Text("VRAM: %dMB / %dMB", g_state.vramUsage, g_state.vramTotal);

			if (config.overlay.debug)
				g_renderer->AppendOverlayDebugInfo();

			position.y += (ImGui::GetWindowSize().y + 10.0f) * direction;
			ImGui::End();
		}
	}

	ImGui::PopStyleColor();
	ImGui::PopFont();
}

void LatteOverlay_RenderNotifications(ImVec2& position, ImVec2& pivot, sint32 direction)
{
	auto& config = GetConfig();

	const auto font = ImGui_GetFont(14.0f * (float)config.notification.text_scale / 100.0f);
	ImGui::PushFont(font);

	const ImVec4 color = ImGui::ColorConvertU32ToFloat4(config.notification.text_color);
	ImGui::PushStyleColor(ImGuiCol_Text, color);

	// selected controller profiles in the beginning
	if (config.notification.controller_profiles)
	{
		static bool s_init_overlay = false;
		if (!s_init_overlay)
		{
			static std::chrono::steady_clock::time_point s_started = tick_cached();

			const auto now = tick_cached();
			if (std::chrono::duration_cast<std::chrono::milliseconds>(now - s_started).count() <= 5000)
			{
				// active account
				ImGui::SetNextWindowPos(position, ImGuiCond_Always, pivot);
				ImGui::SetNextWindowBgAlpha(kBackgroundAlpha);
				if (ImGui::Begin("Active account", nullptr, kPopupFlags))
				{
					ImGui::TextUnformatted((const char*)ICON_FA_USER);
					ImGui::SameLine();

					static std::string s_mii_name;
					if (s_mii_name.empty())
					{
						auto tmp_view = Account::GetAccount(ActiveSettings::GetPersistentId()).GetMiiName();
						std::wstring tmp{ tmp_view };
						s_mii_name = boost::nowide::narrow(tmp);
					}
					ImGui::TextUnformatted(s_mii_name.c_str());

					position.y += (ImGui::GetWindowSize().y + 10.0f) * direction;
					ImGui::End();
				}
				
				// controller
				std::vector<std::pair<int, std::string>> profiles;
				auto& input_manager = InputManager::instance();
				for (int i = 0; i < InputManager::kMaxController; ++i)
				{
					const auto controller = input_manager.get_controller(i);
					if (!controller)
						continue;

					const auto& profile_name = controller->get_profile_name();
					if (profile_name.empty())
						continue;

					profiles.emplace_back(i, profile_name);
				}

				if (!profiles.empty())
				{
					ImGui::SetNextWindowPos(position, ImGuiCond_Always, pivot);
					ImGui::SetNextWindowBgAlpha(kBackgroundAlpha);
					if (ImGui::Begin("Controller profile names", nullptr, kPopupFlags))
					{
						auto it = profiles.cbegin();
						ImGui::TextUnformatted((const char*)ICON_FA_GAMEPAD);
						ImGui::SameLine();
						ImGui::Text("Player %d: %s", it->first + 1, it->second.c_str());

						for (++it; it != profiles.cend(); ++it)
						{
							ImGui::Separator();
							ImGui::TextUnformatted((const char*)ICON_FA_GAMEPAD);
							ImGui::SameLine();
							ImGui::Text("Player %d: %s", it->first + 1, it->second.c_str());
						}

						position.y += (ImGui::GetWindowSize().y + 10.0f) * direction;
						ImGui::End();
					}
				}
				else
					s_init_overlay = true;
			}
			else
				s_init_overlay = true;
		}
	}

	if (config.notification.friends)
	{
		static std::vector< std::pair<std::string, std::chrono::steady_clock::time_point> > s_friend_list;

		std::unique_lock lock(g_friend_notification_mutex);
		if (!g_friend_notifications.empty())
		{
			const auto tick = tick_cached();

			for (const auto& entry : g_friend_notifications)
			{
				s_friend_list.emplace_back(entry.first, tick + std::chrono::milliseconds(entry.second));
			}

			g_friend_notifications.clear();
		}

		if (!s_friend_list.empty())
		{
			ImGui::SetNextWindowPos(position, ImGuiCond_Always, pivot);
			ImGui::SetNextWindowBgAlpha(kBackgroundAlpha);
			if (ImGui::Begin("Friends overlay", nullptr, kPopupFlags))
			{
				const auto tick = tick_cached();
				for (auto it = s_friend_list.cbegin(); it != s_friend_list.cend();)
				{
					ImGui::TextUnformatted(it->first.c_str(), it->first.c_str() + it->first.size());
					if (tick >= it->second)
						it = s_friend_list.erase(it);
					else
						++it;
				}

				position.y += (ImGui::GetWindowSize().y + 10.0f) * direction;
				ImGui::End();
			}


		}
	}

	// low battery warning
	if (config.notification.controller_battery)
	{
		std::vector<int> batteries;
		auto& input_manager = InputManager::instance();
		for (int i = 0; i < InputManager::kMaxController; ++i)
		{
			const auto controller = input_manager.get_controller(i);
			if (!controller)
				continue;

			if (controller->is_battery_low())
				batteries.emplace_back(i);
		}

		if (!batteries.empty())
		{
			static std::chrono::steady_clock::time_point s_last_tick = tick_cached();
			static bool s_blink_state = false;
			const auto now = tick_cached();

			if (std::chrono::duration_cast<std::chrono::milliseconds>(now - s_last_tick).count() >= 750)
			{
				s_blink_state = !s_blink_state;
				s_last_tick = now;
			}

			ImGui::SetNextWindowPos(position, ImGuiCond_Always, pivot);
			ImGui::SetNextWindowBgAlpha(kBackgroundAlpha);
			if (ImGui::Begin("Low battery overlay", nullptr, kPopupFlags))
			{
				auto it = batteries.cbegin();
				ImGui::TextUnformatted((const char*)(s_blink_state ? ICON_FA_BATTERY_EMPTY : ICON_FA_BATTERY_QUARTER));
				ImGui::SameLine();
				ImGui::Text("Player %d", *it + 1);

				for (++it; it != batteries.cend(); ++it)
				{
					ImGui::Separator();
					ImGui::TextUnformatted((const char*)(s_blink_state ? ICON_FA_BATTERY_EMPTY : ICON_FA_BATTERY_QUARTER));
					ImGui::SameLine();
					ImGui::Text("Player %d", *it + 1);
				}

				position.y += (ImGui::GetWindowSize().y + 10.0f) * direction;
				ImGui::End();
			}
		}
	}

	if (config.notification.shader_compiling)
	{
		static int32_t s_shader_count = 0;
		static int32_t s_shader_count_async = 0;
		if (s_shader_count > 0 || g_compiled_shaders_total > 0)
		{
			const int tmp = g_compiled_shaders_total.exchange(0);
			const int tmpAsync = g_compiled_shaders_async.exchange(0);
			s_shader_count += tmp;
			s_shader_count_async += tmpAsync;

			static std::chrono::steady_clock::time_point s_last_tick = tick_cached();
			const auto now = tick_cached();

			if (tmp > 0)
				s_last_tick = now;

			if (std::chrono::duration_cast<std::chrono::milliseconds>(now - s_last_tick).count() >= 2500)
			{
				s_shader_count = 0;
				s_shader_count_async = 0;
			}

			if (s_shader_count > 0)
			{
				ImGui::SetNextWindowPos(position, ImGuiCond_Always, pivot);
				ImGui::SetNextWindowBgAlpha(kBackgroundAlpha);
				if (ImGui::Begin("Compiling shaders overlay", nullptr, kPopupFlags))
				{
					ImRotateStart();
					ImGui::TextUnformatted((const char*)ICON_FA_SPINNER);

					const auto ticks = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
					ImRotateEnd(0.001f * ticks.time_since_epoch().count());
					ImGui::SameLine();

					if (s_shader_count_async > 0 && GetConfig().async_compile) // the latter condition is to never show async count when async isn't enabled. Since it can be confusing to the user
					{
						if(s_shader_count > 1)
							ImGui::Text("Compiled %d new shaders... (%d async)", s_shader_count, s_shader_count_async);
						else
							ImGui::Text("Compiled %d new shader... (%d async)", s_shader_count, s_shader_count_async);
					}
					else
					{
						if (s_shader_count > 1)
							ImGui::Text("Compiled %d new shaders...", s_shader_count);
						else
							ImGui::Text("Compiled %d new shader...", s_shader_count);
					}

					position.y += (ImGui::GetWindowSize().y + 10.0f) * direction;
					ImGui::End();
				}
			}
		}
		
		static int32_t s_pipeline_count = 0;
		static int32_t s_pipeline_count_async = 0;
		if (s_pipeline_count > 0 || g_compiling_pipelines > 0)
		{
			const int tmp = g_compiling_pipelines.exchange(0);
			const int tmpAsync = g_compiling_pipelines_async.exchange(0);
			s_pipeline_count += tmp;
			s_pipeline_count_async += tmpAsync;

			static std::chrono::steady_clock::time_point s_last_tick = tick_cached();
			const auto now = tick_cached();

			if (tmp > 0)
				s_last_tick = now;

			if (std::chrono::duration_cast<std::chrono::milliseconds>(now - s_last_tick).count() >= 2500)
			{
				s_pipeline_count = 0;
				s_pipeline_count_async = 0;
			}

			if (s_pipeline_count > 0)
			{
				ImGui::SetNextWindowPos(position, ImGuiCond_Always, pivot);
				ImGui::SetNextWindowBgAlpha(kBackgroundAlpha);
				if (ImGui::Begin("Compiling pipeline overlay", nullptr, kPopupFlags))
				{
					ImRotateStart();
					ImGui::TextUnformatted((const char*)ICON_FA_SPINNER);

					const auto ticks = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
					ImRotateEnd(0.001f * ticks.time_since_epoch().count());
					ImGui::SameLine();

#ifdef CEMU_DEBUG_ASSERT
					uint64 totalTime = g_compiling_pipelines_syncTimeSum / 1000000ull;
					if (s_pipeline_count_async > 0)
					{
						if (s_pipeline_count > 1)
							ImGui::Text("Compiled %d new pipelines... (%d async) TotalSync: %" PRIu64 "ms", s_pipeline_count, s_pipeline_count_async, totalTime);
						else
							ImGui::Text("Compiled %d new pipeline... (%d async) TotalSync: %" PRIu64 "ms", s_pipeline_count, s_pipeline_count_async, totalTime);
					}
					else
					{
						if (s_pipeline_count > 1)
							ImGui::Text("Compiled %d new pipelines... TotalSync: %" PRIu64 "ms", s_pipeline_count, totalTime);
						else
							ImGui::Text("Compiled %d new pipeline... TotalSync: %" PRIu64 "ms", s_pipeline_count, totalTime);
					}
#else
					if (s_pipeline_count_async > 0)
					{
						if (s_pipeline_count > 1)
							ImGui::Text("Compiled %d new pipelines... (%d async)", s_pipeline_count, s_pipeline_count_async);
						else
							ImGui::Text("Compiled %d new pipeline... (%d async)", s_pipeline_count, s_pipeline_count_async);
					}
					else
					{
						if (s_pipeline_count > 1)
							ImGui::Text("Compiled %d new pipelines...", s_pipeline_count);
						else
							ImGui::Text("Compiled %d new pipeline...", s_pipeline_count);
					}
#endif
					position.y += (ImGui::GetWindowSize().y + 10.0f) * direction;
					ImGui::End();
				}
			}
		}
	}

	// misc notifications
	static std::vector< std::pair<std::string, std::chrono::steady_clock::time_point> > s_misc_notifications;

	std::unique_lock misc_lock(g_notification_mutex);
	if (!g_notifications.empty())
	{
		const auto tick = tick_cached();

		for (const auto& entry : g_notifications)
		{
			s_misc_notifications.emplace_back(entry.first, tick + std::chrono::milliseconds(entry.second));
		}

		g_notifications.clear();
	}
	misc_lock.unlock();

	if (!s_misc_notifications.empty())
	{
		ImGui::SetNextWindowPos(position, ImGuiCond_Always, pivot);
		ImGui::SetNextWindowBgAlpha(kBackgroundAlpha);
		if (ImGui::Begin("Misc notifications", nullptr, kPopupFlags))
		{
			const auto tick = tick_cached();
			for (auto it = s_misc_notifications.cbegin(); it != s_misc_notifications.cend();)
			{
				ImGui::TextUnformatted(it->first.c_str(), it->first.c_str() + it->first.size());
				if (tick >= it->second)
					it = s_misc_notifications.erase(it);
				else
					++it;
			}

			position.y += (ImGui::GetWindowSize().y + 10.0f) * direction;
			ImGui::End();
		}
	}

	ImGui::PopStyleColor();
	ImGui::PopFont();
}

void LatteOverlay_translateScreenPosition(ScreenPosition pos, const Vector2f& window_size, ImVec2& position, ImVec2& pivot, sint32& direction)
{
	switch (pos)
	{
	case ScreenPosition::kTopLeft:
		position = { 10, 10 };
		pivot = { 0, 0 };
		direction = 1;
		break;
	case ScreenPosition::kTopCenter:
		position = { window_size.x / 2.0f, 10 };
		pivot = { 0.5f, 0 };
		direction = 1;
		break;
	case ScreenPosition::kTopRight:
		position = { window_size.x - 10, 10 };
		pivot = { 1, 0 };
		direction = 1;
		break;
	case ScreenPosition::kBottomLeft:
		position = { 10, window_size.y - 10 };
		pivot = { 0, 1 };
		direction = -1;
		break;
	case ScreenPosition::kBottomCenter:
		position = { window_size.x / 2.0f, window_size.y - 10 };
		pivot = { 0.5f, 1 };
		direction = -1;
		break;
	case ScreenPosition::kBottomRight:
		position = { window_size.x - 10, window_size.y - 10 };
		pivot = { 1, 1 };
		direction = -1;
		break;
	default:
		UNREACHABLE;
	}
}

void LatteOverlay_render(bool pad_view)
{
	const auto& config = GetConfig();
	if(config.overlay.position == ScreenPosition::kDisabled && config.notification.position == ScreenPosition::kDisabled)
		return;

	sint32 w = 0, h = 0;
	if (pad_view && gui_isPadWindowOpen())
		gui_getPadWindowSize(&w, &h);
	else
		gui_getWindowSize(&w, &h);

	if (w == 0 || h == 0)
		return;

	const Vector2f window_size{ (float)w,(float)h };
	
	// test if fonts are already precached
	if (!ImGui_GetFont(14.0f * (float)config.overlay.text_scale / 100.0f))
		return;
	
	if (!ImGui_GetFont(14.0f * (float)config.notification.text_scale / 100.0f))
		return;

	ImVec2 position{}, pivot{};
	sint32 direction{};

	if (config.overlay.position != ScreenPosition::kDisabled)
	{
		LatteOverlay_translateScreenPosition(config.overlay.position, window_size, position, pivot, direction);
		LatteOverlay_renderOverlay(position, pivot, direction);
	}
	

	if (config.notification.position != ScreenPosition::kDisabled)
	{
		if(config.overlay.position != config.notification.position)
			LatteOverlay_translateScreenPosition(config.notification.position, window_size, position, pivot, direction);

		LatteOverlay_RenderNotifications(position, pivot, direction);
	}
}


void LatteOverlay_init()
{
#if BOOST_OS_WINDOWS
	SYSTEM_INFO sys_info;
	GetSystemInfo(&sys_info);
	g_state.processor_count = sys_info.dwNumberOfProcessors;

	g_state.processor_times.resize(g_state.processor_count);
	g_state.cpu_per_core.resize(g_state.processor_count);
#else
	g_state.processor_count = 1;
#endif
}

void LatteOverlay_updateStats(double fps, sint32 drawcalls)
{
	if (GetConfig().overlay.position == ScreenPosition::kDisabled)
		return;

	g_state.fps = fps;
	g_state.draw_calls_per_frame = drawcalls;

#if BOOST_OS_WINDOWS
	// update cemu cpu
	FILETIME ftime, fkernel, fuser;
	LARGE_INTEGER now, kernel, user;
	GetSystemTimeAsFileTime(&ftime);
	now.LowPart = ftime.dwLowDateTime;
	now.HighPart = ftime.dwHighDateTime;

	GetProcessTimes(GetCurrentProcess(), &ftime, &ftime, &fkernel, &fuser);
	kernel.LowPart = fkernel.dwLowDateTime;
	kernel.HighPart = fkernel.dwHighDateTime;

	user.LowPart = fuser.dwLowDateTime;
	user.HighPart = fuser.dwHighDateTime;

	double percent = (kernel.QuadPart - g_state.kernel) + (user.QuadPart - g_state.user);
	percent /= (now.QuadPart - g_state.last_cpu);
	percent /= g_state.processor_count;
	g_state.cpu_usage = percent * 100;
	g_state.last_cpu = now.QuadPart;
	g_state.user = user.QuadPart;
	g_state.kernel = kernel.QuadPart;

	// update cpu per core
	std::vector<SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION> sppi(g_state.processor_count);
	if (NT_SUCCESS(NtQuerySystemInformation(SystemProcessorPerformanceInformation, sppi.data(), sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION) * g_state.processor_count, nullptr)))
	{
		for (sint32 i = 0; i < g_state.processor_count; ++i)
		{
			const uint64 kernel_diff = sppi[i].KernelTime.QuadPart - g_state.processor_times[i].kernel;
			const uint64 user_diff = sppi[i].UserTime.QuadPart - g_state.processor_times[i].user;
			const uint64 idle_diff = sppi[i].IdleTime.QuadPart - g_state.processor_times[i].idle;

			const auto total = kernel_diff + user_diff; // kernel time already includes idletime
			const double cpu = total == 0 ? 0 : (1.0 - ((double)idle_diff / total)) * 100.0;

			g_state.cpu_per_core[i] = cpu;
			//total_cpu += cpu;

			g_state.processor_times[i].idle = sppi[i].IdleTime.QuadPart;
			g_state.processor_times[i].kernel = sppi[i].KernelTime.QuadPart;
			g_state.processor_times[i].user = sppi[i].UserTime.QuadPart;
		}

		//total_cpu /= g_state.processor_count;
		//g_state.cpu_usage = total_cpu;
	}

	// update ram
	PROCESS_MEMORY_COUNTERS pmc{};
	pmc.cb = sizeof(pmc);
	GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
	g_state.ram_usage = (pmc.WorkingSetSize / 1000) / 1000;
#endif

	// update vram
	g_renderer->GetVRAMInfo(g_state.vramUsage, g_state.vramTotal);
}

void LatteOverlay_updateStatsPerFrame()
{
	if (!ActiveSettings::FrameProfilerEnabled())
		return;
	// update frametime graph
	uint32 frameTime_total = (uint32)PPCTimer_tscToMicroseconds(performanceMonitor.gpuTime_frameTime.getPreviousFrameValue());
	uint32 frameTime_idle = (uint32)PPCTimer_tscToMicroseconds(performanceMonitor.gpuTime_idleTime.getPreviousFrameValue());
	uint32 frameTime_dcStageTextures = (uint32)PPCTimer_tscToMicroseconds(performanceMonitor.gpuTime_dcStageTextures.getPreviousFrameValue());
	uint32 frameTime_dcStageVertexMgr = (uint32)PPCTimer_tscToMicroseconds(performanceMonitor.gpuTime_dcStageVertexMgr.getPreviousFrameValue());
	uint32 frameTime_dcStageShaderAndUniformMgr = (uint32)PPCTimer_tscToMicroseconds(performanceMonitor.gpuTime_dcStageShaderAndUniformMgr.getPreviousFrameValue());
	uint32 frameTime_dcStageIndexMgr = (uint32)PPCTimer_tscToMicroseconds(performanceMonitor.gpuTime_dcStageIndexMgr.getPreviousFrameValue());
	uint32 frameTime_dcStageMRT = (uint32)PPCTimer_tscToMicroseconds(performanceMonitor.gpuTime_dcStageMRT.getPreviousFrameValue());
	uint32 frameTime_dcStageDrawcallAPI = (uint32)PPCTimer_tscToMicroseconds(performanceMonitor.gpuTime_dcStageDrawcallAPI.getPreviousFrameValue());
	uint32 frameTime_waitForAsync = (uint32)PPCTimer_tscToMicroseconds(performanceMonitor.gpuTime_waitForAsync.getPreviousFrameValue());

	// make sure total frame time is not less than it's sums
	uint32 minimumExpectedFrametime =
		frameTime_idle +
		frameTime_dcStageTextures +
		frameTime_dcStageVertexMgr +
		frameTime_dcStageShaderAndUniformMgr +
		frameTime_dcStageIndexMgr +
		frameTime_dcStageMRT +
		frameTime_dcStageDrawcallAPI +
		frameTime_waitForAsync;
	frameTime_total = std::max(frameTime_total, minimumExpectedFrametime);

	//g_state.frametimeGraph.appendEntry();
	//g_state.frametimeGraph.setCurrentEntryValue(0xFF404040, frameTime_idle);
	//g_state.frametimeGraph.setCurrentEntryValue(0xFFFFC0FF, frameTime_waitForAsync);
	//g_state.frametimeGraph.setCurrentEntryValue(0xFF000040, frameTime_dcStageTextures); // dark red
	//g_state.frametimeGraph.setCurrentEntryValue(0xFF004000, frameTime_dcStageVertexMgr); // dark green
	//g_state.frametimeGraph.setCurrentEntryValue(0xFFFFFF80, frameTime_dcStageShaderAndUniformMgr); // blueish
	//g_state.frametimeGraph.setCurrentEntryValue(0xFF800080, frameTime_dcStageIndexMgr); // purple
	//g_state.frametimeGraph.setCurrentEntryValue(0xFF00FF00, frameTime_dcStageMRT); // green
	//g_state.frametimeGraph.setCurrentEntryValue(0xFF00FFFF, frameTime_dcStageDrawcallAPI); // yellow
	//g_state.frametimeGraph.setCurrentEntryValue(0xFFBBBBBB, frameTime_total - minimumExpectedFrametime);
}
