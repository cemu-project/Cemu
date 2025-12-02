#pragma once

namespace coreinit
{
	enum class RplEntryReason;
};

// base class for HLE RPL implementations
class COSModule
{
public:
	virtual std::string_view GetName() = 0;

	virtual std::vector<std::string_view> GetDependencies() { return {}; };

	virtual void RPLMapped() {}; // RPL mapped into process
	virtual void RPLUnmapped() {}; // RPL unmapped

	virtual void rpl_entry(uint32 moduleHandle, coreinit::RplEntryReason reason) {};
	// note: to simplify cleanup, both RPLUnmapped() and rpl_entry(unload) are always called even if the process is shutdown via "Close game"
};

std::span<COSModule*> GetCOSModules();
