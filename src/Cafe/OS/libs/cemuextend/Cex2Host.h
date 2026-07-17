#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

class ModExecutionContext;
struct VPADStatus;

namespace cemuextend_hle {

class Cex2Host
{
public:
	static Cex2Host& Instance();

	std::int32_t Query(ModExecutionContext& owner, std::uint32_t query,
		std::span<std::byte> output);
	std::int32_t Open(ModExecutionContext& owner, std::span<const std::byte> options,
		std::uint32_t& sessionId);
	std::int32_t Submit(ModExecutionContext& owner, std::uint32_t sessionId,
		std::span<const std::byte> request);
	std::int32_t Poll(ModExecutionContext& owner, std::uint32_t sessionId,
		std::span<std::byte> output, std::uint32_t& outputSize);
	std::int32_t Cancel(ModExecutionContext& owner, std::uint32_t sessionId,
		std::uint32_t correlationId);
	std::int32_t Close(ModExecutionContext& owner, std::uint32_t sessionId);
	void CloseOwner(ModExecutionContext& owner);
	void CloseAll();
	void ObserveVpad(std::int32_t channel, const VPADStatus& status, std::int32_t error,
		std::int32_t sampleCount);
	void ApplyMappedVpad(std::int32_t channel, VPADStatus& status);
	void KeyboardEvent(std::uint16_t usbHidUsage, bool pressed, std::uint8_t modifiers);
	void KeyboardFocusLost();
	void PermissionsChanged(ModExecutionContext& owner, std::uint32_t permissions);

private:
	struct Impl;
	Cex2Host();
	~Cex2Host();
	Cex2Host(const Cex2Host&) = delete;
	Cex2Host& operator=(const Cex2Host&) = delete;
	std::unique_ptr<Impl> m_impl;
};

} // namespace cemuextend_hle
