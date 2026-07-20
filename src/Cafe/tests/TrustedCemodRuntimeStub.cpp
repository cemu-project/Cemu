#include "Cafe/HW/Espresso/TrustedCemodRuntime.h"

struct TrustedCemodRuntime::Impl {};

TrustedCemodRuntime::TrustedCemodRuntime() : m_impl(std::make_unique<Impl>()) {}
TrustedCemodRuntime::~TrustedCemodRuntime() = default;

std::optional<std::uint64_t> TrustedCemodRuntime::Load(CemodPackage,
	std::uint32_t, const ModServicePermissions&, std::string& error)
{
	error = "trusted runtime is not linked into isolated runtime unit tests";
	return std::nullopt;
}

bool TrustedCemodRuntime::Unload(std::uint64_t) { return false; }
void TrustedCemodRuntime::UnloadAll() {}
void TrustedCemodRuntime::UpdatePermissions(std::uint32_t, const ModServicePermissions&) {}
cemuextend_hle::Cex2Owner* TrustedCemodRuntime::Owner() { return nullptr; }
std::size_t TrustedCemodRuntime::Size() const { return 0; }
