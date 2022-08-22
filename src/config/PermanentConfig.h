#pragma once

#include "PermanentStorage.h"

struct PermanentConfig
{
	static constexpr const char* kFileName = "perm_setting.xml";
	
	std::string custom_mlc_path;

	[[nodiscard]] std::string ToXMLString() const noexcept;
	static PermanentConfig FromXMLString(std::string_view str) noexcept;

	// gets from permanent storage
	static PermanentConfig Load();
	// saves to permanent storage
	bool Store() noexcept;
};
