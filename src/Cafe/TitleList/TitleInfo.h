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

enum class CosCapabilityGroup : uint32
{
	None = 0,
	BSP  =  1,
	DK   =  3,
	USB  =  9,
	UHS  = 12,
	FS   = 11,
	MCP  = 13,
	NIM  = 14,
	ACT  = 15,
	FPD  = 16,
	BOSS = 17,
	ACP  = 18,
	PDM  = 19,
	AC   = 20,
	NDM  = 21,
	NSEC = 22
};

enum class CosCapabilityBits : uint64
{
	All = 0xFFFFFFFFFFFFFFFFull
};

enum class CosCapabilityBitsFS : uint64
{
	ODD_READ         = (1llu << 0),
	ODD_WRITE        = (1llu << 1),
	ODD_RAW_OPEN     = (1llu << 2),
	ODD_MOUNT        = (1llu << 3),
	SLCCMPT_READ     = (1llu << 4),
	SLCCMPT_WRITE    = (1llu << 5),
	SLCCMPT_RAW_OPEN = (1llu << 6),
	SLCCMPT_MOUNT    = (1llu << 7),
	SLC_READ         = (1llu << 8),
	SLC_WRITE        = (1llu << 9),
	SLC_RAW_OPEN     = (1llu << 10),
	SLC_MOUNT        = (1llu << 11),
	MLC_READ         = (1llu << 12),
	MLC_WRITE        = (1llu << 13),
	MLC_RAW_OPEN     = (1llu << 14),
	MLC_MOUNT        = (1llu << 15),
	SDCARD_READ      = (1llu << 16),
	SDCARD_WRITE     = (1llu << 17),
	SDCARD_RAW_OPEN  = (1llu << 18),
	SDCARD_MOUNT     = (1llu << 19),
	HFIO_READ        = (1llu << 20),
	HFIO_WRITE       = (1llu << 21),
	HFIO_RAW_OPEN    = (1llu << 22),
	HFIO_MOUNT       = (1llu << 23),
	RAMDISK_READ     = (1llu << 24),
	RAMDISK_WRITE    = (1llu << 25),
	RAMDISK_RAW_OPEN = (1llu << 26),
	RAMDISK_MOUNT    = (1llu << 27),
	USB_READ         = (1llu << 28),
	USB_WRITE        = (1llu << 29),
	USB_RAW_OPEN     = (1llu << 30),
	USB_MOUNT        = (1llu << 31),
	OTHER_READ       = (1llu << 32),
	OTHER_WRITE      = (1llu << 33),
	OTHER_RAW_OPEN   = (1llu << 34),
	OTHER_MOUNT      = (1llu << 35)
};
ENABLE_BITMASK_OPERATORS(CosCapabilityBitsFS);

struct ParsedCosXml 
{
  public:

	std::string argstr;

	struct Permission
	{
		CosCapabilityGroup group{CosCapabilityGroup::None};
		CosCapabilityBits mask{CosCapabilityBits::All};
	};
	Permission permissions[19]{};

	static ParsedCosXml* Parse(uint8* xmlData, size_t xmlLen);

	CosCapabilityBits GetCapabilityBits(CosCapabilityGroup group) const
	{
		for (const auto& perm : permissions)
		{
			if (perm.group == group)
				return perm.mask;
		}
		return CosCapabilityBits::All;
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
	  	NUS = 4, // NUS format. Directory with .app files, title.tik and title.tmd
	  	WUHB = 5,
		// error
		INVALID_STRUCTURE = 0,
	};

	enum class InvalidReason : uint8
	{
		NONE = 0,
		BAD_PATH_OR_INACCESSIBLE = 1,
		UNKNOWN_FORMAT = 2,
		NO_DISC_KEY = 3,
		NO_TITLE_TIK = 4,
		MISSING_XML_FILES = 4,
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
	InvalidReason GetInvalidReason() const { return m_invalidReason; }
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

	// meta.xml also contains a version field which seems to match the one from app.xml
	// the titleId in meta.xml seems to be the title id of the base game for updates specifically. For AOC content it's the AOC's titleId

	TitleIdParser::TITLE_TYPE GetTitleType();
	ParsedMetaXml* GetMetaInfo()
	{
		return m_parsedMetaXml;
	}

	ParsedCosXml* GetCosInfo()
	{
		return m_parsedCosXml;
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
	void SetInvalidReason(InvalidReason reason);
	ParsedMetaXml* ParseAromaIni(std::span<unsigned char> content);
	bool ParseAppXml(std::vector<uint8>& appXmlData);

	bool m_isValid{ false };
	TitleDataFormat m_titleFormat{ TitleDataFormat::INVALID_STRUCTURE };
	fs::path m_fullPath;
	std::string m_subPath; // used for formats where fullPath isn't unique on its own (like WUA)
	uint64 m_uid{};
	InvalidReason m_invalidReason{ InvalidReason::NONE }; // if m_isValid == false, this contains a more detailed error code
	// mounting info
	std::vector<std::pair<sint32, std::string>> m_mountpoints;
	class FSTVolume* m_wudVolume{};
	class ZArchiveReader* m_zarchive{};
	class WUHBReader* m_wuhbreader{};
	// xml info
	bool m_hasParsedXmlFiles{ false };
	ParsedMetaXml* m_parsedMetaXml{};
	ParsedAppXml* m_parsedAppXml{};
	ParsedCosXml* m_parsedCosXml{};
	// cached info if called with cache constructor
	CachedInfo* m_cachedInfo{nullptr};
};