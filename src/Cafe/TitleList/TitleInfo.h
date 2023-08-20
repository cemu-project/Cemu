#pragma once

#include "Cafe/Filesystem/fsc.h"
#include "config/CemuConfig.h" // for CafeConsoleRegion. Move to NCrypto?
#include "TitleId.h"
#include "AppType.h"
#include "ParsedMetaXml.h"

enum class CafeTitleFileType
{
	UNKNOWN,
	WUD,
	WUX,
	RPX,
	ELF,
};

CafeTitleFileType DetermineCafeSystemFileType(fs::path filePath);

struct ParsedAppXml
{
	uint64 title_id;
	uint16 title_version;
	uint32 app_type;
	uint32 group_id;
	uint32 sdk_version;
};

struct ParsedCosXml 
{
	std::string argstr;

	static ParsedCosXml* Parse(uint8* xmlData, size_t xmlLen)
	{
		pugi::xml_document app_doc;
		if (!app_doc.load_buffer_inplace(xmlData, xmlLen))
			return nullptr;

		const auto root = app_doc.child("app");
		if (!root)
			return nullptr;

		ParsedCosXml* parsedCos = new ParsedCosXml();

		for (const auto& child : root.children())
		{
			std::string_view name = child.name();
			if (name == "argstr")
				parsedCos->argstr = child.text().as_string();
		}
		return parsedCos;
	}
};

class TitleInfo
{
public:
	enum class TitleDataFormat
	{
		HOST_FS = 1, // host filesystem directory (fullPath points to root with content/code/meta subfolders)
		WUD = 2, // WUD or WUX
		WIIU_ARCHIVE = 3, // Wii U compressed single-file archive (.wua)
		// error
		INVALID_STRUCTURE = 0,
	};

	struct CachedInfo
	{
		TitleDataFormat titleDataFormat;
		fs::path path;
		std::string subPath; // for WUA
		uint64 titleId;
		uint16 titleVersion;
		uint32 sdkVersion;
		std::string titleName;
		CafeConsoleRegion region;
		uint32 group_id;
		uint32 app_type;
	};

	TitleInfo() : m_isValid(false) {};
	TitleInfo(const fs::path& path);
	TitleInfo(const fs::path& path, std::string_view subPath);
	TitleInfo(const CachedInfo& cachedInfo);
	~TitleInfo();

	TitleInfo(const TitleInfo& other)
	{
		Copy(other);
	}

	TitleInfo& operator=(TitleInfo other)
	{
		Copy(other);
		return *this;
	}

	bool IsCached() { return m_cachedInfo; }; // returns true if this TitleInfo was loaded from cache and has not yet been parsed

	CachedInfo MakeCacheEntry();

	bool IsValid() const;
	uint64 GetUID(); // returns a unique identifier derived from the absolute canonical title location which can be used to identify this title by its location. May not persist across sessions, especially when Cemu is used portable

	fs::path GetPath() const;
	TitleDataFormat GetFormat() const { return m_titleFormat; };

	bool Mount(std::string_view virtualPath, std::string_view subfolder, sint32 mountPriority);
	void Unmount(std::string_view virtualPath);
	void UnmountAll();
	bool IsMounted() const { return !m_mountpoints.empty(); }

	bool ParseXmlInfo();
	bool HasValidXmlInfo() const { return m_parsedMetaXml && m_parsedAppXml && m_parsedCosXml; };

	bool IsEqualByLocation(const TitleInfo& rhs) const
	{
		return m_uid == rhs.m_uid;
	}

	bool IsSystemDataTitle() const
	{
		if(!IsValid())
			return false;
		uint32 appType = GetAppType();
		return appType == APP_TYPE::DRC_FIRMWARE || appType == APP_TYPE::DRC_TEXTURE_ATLAS || appType == APP_TYPE::VERSION_DATA_TITLE;
	}

	// API which requires parsed meta data or cached info
	TitleId GetAppTitleId() const; // from app.xml
	uint16 GetAppTitleVersion() const; // from app.xml
	uint32 GetAppSDKVersion() const; // from app.xml
	uint32 GetAppGroup() const; // from app.xml
	uint32 GetAppType() const; // from app.xml
	std::string GetMetaTitleName() const; // from meta.xml
	CafeConsoleRegion GetMetaRegion() const; // from meta.xml
	uint32 GetOlvAccesskey() const;

	// cos.xml
	std::string GetArgStr() const;

	// meta.xml also contains a version which seems to match the one from app.xml
	// the titleId in meta.xml seems to be the title id of the base game for updates specifically. For AOC content it's the AOC's titleId

	TitleIdParser::TITLE_TYPE GetTitleType();
	ParsedMetaXml* GetMetaInfo()
	{
		return m_parsedMetaXml;
	}

	std::string GetPrintPath() const; // formatted path including type and WUA subpath. Intended for logging and user-facing information
	std::string GetInstallPath() const; // installation subpath, relative to storage base. E.g. "usr/title/.../..." or "sys/title/.../..."

	static std::string GetUniqueTempMountingPath();
	static bool ParseWuaTitleFolderName(std::string_view name, TitleId& titleIdOut, uint16& titleVersionOut);

private:
	void Copy(const TitleInfo& other)
	{
		m_isValid = other.m_isValid;
		m_titleFormat = other.m_titleFormat;
		m_fullPath = other.m_fullPath;
		m_subPath = other.m_subPath;
		m_hasParsedXmlFiles = other.m_hasParsedXmlFiles;
		m_parsedMetaXml = nullptr;
		m_parsedAppXml = nullptr;

		if (other.m_parsedMetaXml)
			m_parsedMetaXml = new ParsedMetaXml(*other.m_parsedMetaXml);
		if (other.m_parsedAppXml)
			m_parsedAppXml = new ParsedAppXml(*other.m_parsedAppXml);
		if (other.m_parsedCosXml)
			m_parsedCosXml = new ParsedCosXml(*other.m_parsedCosXml);

		if (other.m_cachedInfo)
			m_cachedInfo = new CachedInfo(*other.m_cachedInfo);

		m_mountpoints.clear();
		m_wudVolume = nullptr;
	}

	bool DetectFormat(const fs::path& path, fs::path& pathOut, TitleDataFormat& formatOut);
	void CalcUID();

	bool ParseAppXml(std::vector<uint8>& appXmlData);

	bool m_isValid{ false };
	TitleDataFormat m_titleFormat{ TitleDataFormat::INVALID_STRUCTURE };
	fs::path m_fullPath;
	std::string m_subPath; // used for formats where fullPath isn't unique on its own (like WUA)
	uint64 m_uid{};
	// mounting info
	std::vector<std::pair<sint32, std::string>> m_mountpoints;
	class FSTVolume* m_wudVolume{};
	class ZArchiveReader* m_zarchive{};
	// xml info
	bool m_hasParsedXmlFiles{ false };
	ParsedMetaXml* m_parsedMetaXml{};
	ParsedAppXml* m_parsedAppXml{};
	ParsedCosXml* m_parsedCosXml{};
	// cached info if called with cache constructor
	CachedInfo* m_cachedInfo{nullptr};
};