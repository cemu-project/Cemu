#pragma once

#include "BaseInfo.h"
#include "config/CemuConfig.h"

class MetaInfo : public BaseInfo
{
public:
	MetaInfo();
	MetaInfo(const fs::path& filename);

	// returns long name with replaces newlines to ' - '
	[[nodiscard]] std::string GetName(CafeConsoleLanguage language = CafeConsoleLanguage::EN) const;
	
	[[nodiscard]] uint64 GetTitleId() const { return m_title_id; }
	[[nodiscard]] uint32 GetTitleIdHigh() const { return (uint32)(GetTitleId() >> 32); }
	[[nodiscard]] uint32 GetTitleIdLow() const { return (uint32)(GetTitleId() & 0xFFFFFFFF); }
	
	[[nodiscard]] uint64 GetBaseTitleId() const { return m_title_id & ~0xF00000000ULL; }
	[[nodiscard]] uint32 GetBaseTitleIdHigh() const { return (uint32)(GetBaseTitleId() >> 32); }
	[[nodiscard]] uint32 GetBaseTitleIdLow() const { return (uint32)(GetBaseTitleId() & 0xFFFFFFFF); }

	[[nodiscard]] uint64 GetUpdateTitleId() const { return GetBaseTitleId() | 0xE00000000ULL; }
	[[nodiscard]] uint32 GetUpdateTitleIdHigh() const { return (uint32)(GetUpdateTitleId() >> 32); }
	[[nodiscard]] uint32 GetUpdateTitleIdLow() const { return (uint32)(GetUpdateTitleId() & 0xFFFFFFFF); }

	[[nodiscard]] uint64 GetDLCTitleId() const { return GetBaseTitleId() | 0xC00000000ULL; }
	[[nodiscard]] uint32 GetDLCTitleIdHigh() const { return (uint32)(GetDLCTitleId() >> 32); }
	[[nodiscard]] uint32 GetDLCTitleIdLow() const { return (uint32)(GetDLCTitleId() & 0xFFFFFFFF); }

	[[nodiscard]] const std::string& GetLongName(CafeConsoleLanguage language) const;
	[[nodiscard]] const std::string& GetShortName(CafeConsoleLanguage language) const;
	[[nodiscard]] const std::string& GetPublisher(CafeConsoleLanguage language) const;
	[[nodiscard]] const std::string& GetProductCode() const { return m_product_code; }
	[[nodiscard]] const std::string& GetCompanyCode() const { return m_company_code; }
	[[nodiscard]] const std::string& GetContentPlatform() const { return m_content_platform; }
	[[nodiscard]] uint32 GetVersion() const { return m_version; }
	[[nodiscard]] CafeConsoleRegion GetRegion() const { return m_region; }

	[[nodiscard]] std::unique_ptr<uint8[]> GetIcon(uint32& size) const;

protected:
	// meta.xml
	uint32 m_version;
	std::string m_product_code;
	std::string m_company_code;
	std::string m_content_platform;
	uint64 m_title_id;
	CafeConsoleRegion m_region;

	std::array<std::string, 12> m_long_name;
	std::array<std::string, 12> m_short_name;
	std::array<std::string, 12> m_publisher;

	void ParseDirectory(const fs::path& filename) override;
	bool ParseFile(const fs::path& filename) override;

	void ParseMetaFile(const fs::path& meta_file);
	void ParseMetaFile(const pugi::xml_document& doc, const pugi::xml_parse_result& result);
};

