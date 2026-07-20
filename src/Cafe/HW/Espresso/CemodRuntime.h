#pragma once

#include "Cafe/HW/Espresso/CemodPackage.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

struct ModExecutionContext;
struct ModServicePermissions;
struct PPCInterpreter_t;
namespace cemuextend_hle { class Cex2Owner; }

enum class CemodLifecycle : std::uint8_t { Init, Tick, Event, Shutdown };

class CemodRuntime
{
public:
	static constexpr std::size_t kMaximumModsPerTitle = 16;
	static constexpr std::uint32_t kMaximumTitleTimeMicroseconds = 4000;

	CemodRuntime();
	~CemodRuntime();

	[[nodiscard]] std::optional<std::uint64_t> Load(CemodPackage package,
		std::uint32_t userPermissions, std::uint32_t titlePermissions, std::string& error,
		const ModServicePermissions* servicePermissions = nullptr);
	[[nodiscard]] bool Invoke(std::uint64_t handle, CemodLifecycle lifecycle,
		std::uint32_t argument = 0, std::uint32_t argumentSize = 0);
	[[nodiscard]] bool Unload(std::uint64_t handle);
	void BeginFrame();
	void TickAll();
	void EventAll(std::uint32_t event);
	void UpdatePermissions(std::string_view principal, std::uint32_t permissions,
		const ModServicePermissions& services);
	void UpdateTitlePermissions(const ModServicePermissions& services);
	void UnloadAll();

	[[nodiscard]] ModExecutionContext* Context(std::uint64_t handle);
	[[nodiscard]] PPCInterpreter_t* Cpu(std::uint64_t handle);
	[[nodiscard]] std::size_t Size() const;
	[[nodiscard]] cemuextend_hle::Cex2Owner* TrustedOwner();

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
};
