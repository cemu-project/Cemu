#pragma once

#include "Cafe/HW/Espresso/CemodPackage.h"
#include "Cafe/HW/Espresso/ModExecutionContext.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace cemuextend_hle { class Cex2Owner; }

class TrustedCemodRuntime
{
public:
	TrustedCemodRuntime();
	~TrustedCemodRuntime();

	[[nodiscard]] std::optional<std::uint64_t> Load(CemodPackage package,
		std::uint32_t titlePermissions, const ModServicePermissions& services,
		std::string& error);
	[[nodiscard]] bool Unload(std::uint64_t handle);
	void UnloadAll();
	void UpdatePermissions(std::uint32_t permissions, const ModServicePermissions& services);

	[[nodiscard]] cemuextend_hle::Cex2Owner* Owner();
	[[nodiscard]] std::size_t Size() const;

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
};
