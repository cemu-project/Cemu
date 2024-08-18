#include "TitleInfo.h"
#include "Cafe/Filesystem/fscDeviceHostFS.h"
#include "Cafe/Filesystem/WUHB/WUHBReader.h"
#include "Cafe/Filesystem/FST/FST.h"
#include "pugixml.hpp"
#include "Common/FileStream.h"
#include <zarchive/zarchivereader.h>
#include "util/IniParser/IniParser.h"
#include "util/crypto/crc32.h"
#include "config/ActiveSettings.h"
#include "util/helpers/helpers.h"

// detect format by reading file header/footer
CafeTitleFileType DetermineCafeSystemFileType(fs::path filePath)
{
	std::unique_ptr<FileStream> fs(FileStream::openFile2(filePath));
	if (!fs)
		return CafeTitleFileType::UNKNOWN;
	// very small files (<32 bytes) are always considered unknown
	uint64 fileSize = fs->GetSize();
	if (fileSize < 32)
		return CafeTitleFileType::UNKNOWN;
	// read header bytes
	uint8 headerRaw[32]{};
	fs->readData(headerRaw, sizeof(headerRaw));
	// check for WUX
	uint8 wuxHeaderMagic[8] = { 0x57,0x55,0x58,0x30,0x2E,0xD0,0x99,0x10 };
	if (memcmp(headerRaw, wuxHeaderMagic, sizeof(wuxHeaderMagic)) == 0)
		return CafeTitleFileType::WUX;
	// check for RPX
	uint8 rpxHeaderMagic[9] = { 0x7F,0x45,0x4C,0x46,0x01,0x02,0x01,0xCA,0xFE };
	if (memcmp(headerRaw, rpxHeaderMagic, sizeof(rpxHeaderMagic)) == 0)
		return CafeTitleFileType::RPX;
	// check for ELF
	uint8 elfHeaderMagic[9] = { 0x7F,0x45,0x4C,0x46,0x01,0x02,0x01,0x00,0x00 };
	if (memcmp(headerRaw, elfHeaderMagic, sizeof(elfHeaderMagic)) == 0)
		return CafeTitleFileType::ELF;
	// check for WUD
	uint8 wudMagic1[4] = { 0x57,0x55,0x50,0x2D }; // wud files should always start with "WUP-..."
	uint8 wudMagic2[4] = { 0xCC,0x54,0x9E,0xB9 };
	if (fileSize >= 0x10000)
	{
		uint8 magic1[4];
		fs->SetPosition(0);
		fs->readData(magic1, 4);
		if (memcmp(magic1, wudMagic1, 4) == 0)
		{
			uint8 magic2[4];
			fs->SetPosition(0x10000);
			fs->readData(magic2, 4);
			if (memcmp(magic2, wudMagic2, 4) == 0)
			{
				return CafeTitleFileType::WUD;
			}
		}
	}
	// check for WUA
	// todo
	return CafeTitleFileType::UNKNOWN;
}

TitleInfo::TitleInfo(const fs::path& path)
{
	m_isValid = DetectFormat(path, m_fullPath, m_titleFormat);
	if (!m_isValid)
		m_titleFormat = TitleDataFormat::INVALID_STRUCTURE;
	else
	{
		m_isValid = ParseXmlInfo();
	}
	if (m_isValid)
		CalcUID();
}

TitleInfo::TitleInfo(const fs::path& path, std::string_view subPath)
{
	// path must point to a (wua) file
	if (!path.has_filename())
	{
		m_isValid = false;
		SetInvalidReason(InvalidReason::BAD_PATH_OR_INACCESSIBLE);
		return;
	}
	m_isValid = true;
	m_titleFormat = TitleDataFormat::WIIU_ARCHIVE;
	m_fullPath = path;
	m_subPath = subPath;
	m_isValid = ParseXmlInfo();
	if (m_isValid)
		CalcUID();
}

TitleInfo::TitleInfo(const TitleInfo::CachedInfo& cachedInfo)
{
	m_cachedInfo = new CachedInfo(cachedInfo);
	m_fullPath = cachedInfo.path;
	m_subPath = cachedInfo.subPath;
	m_titleFormat = cachedInfo.titleDataFormat;
	// verify some parameters
	m_isValid = false;
	if (cachedInfo.titleDataFormat != TitleDataFormat::HOST_FS &&
		cachedInfo.titleDataFormat != TitleDataFormat::WIIU_ARCHIVE &&
		cachedInfo.titleDataFormat != TitleDataFormat::WUHB &&
		cachedInfo.titleDataFormat != TitleDataFormat::WUD &&
		cachedInfo.titleDataFormat != TitleDataFormat::NUS &&
		cachedInfo.titleDataFormat != TitleDataFormat::INVALID_STRUCTURE)
		return;
	if (cachedInfo.path.empty())
		return;
	if (cachedInfo.titleDataFormat == TitleDataFormat::WIIU_ARCHIVE && m_subPath.empty())
		return; // for wua files the subpath must never be empty (title must not be stored in root of archive)
	m_isValid = true;
	CalcUID();
}

TitleInfo::~TitleInfo()
{
	cemu_assert(m_mountpoints.empty());
	delete m_parsedMetaXml;
	delete m_parsedAppXml;
	delete m_parsedCosXml;
	delete m_cachedInfo;
}

TitleInfo::CachedInfo TitleInfo::MakeCacheEntry()
{
	cemu_assert_debug(IsValid());
	CachedInfo e;
	e.titleDataFormat = m_titleFormat;
	e.path = m_fullPath;
	e.subPath = m_subPath;
	e.titleId = GetAppTitleId();
	e.titleVersion = GetAppTitleVersion();
	e.sdkVersion = GetAppSDKVersion();
	e.titleName = GetMetaTitleName();
	e.region = GetMetaRegion();
	e.group_id = GetAppGroup();
	e.app_type = GetAppType();
	return e;
}

// WUA can contain multiple titles. Root directory contains one directory for each title. The name must match: <titleId>_v<version>
bool TitleInfo::ParseWuaTitleFolderName(std::string_view name, TitleId& titleIdOut, uint16& titleVersionOut)
{
	std::string_view sv = name;
	if (sv.size() < 16 + 2)
		return false;
	TitleId parsedId;
	if (!TitleIdParser::ParseFromStr(sv, parsedId))
		return false;
	sv.remove_prefix(16);
	if (sv[0] != '_' || (sv[1] != 'v' && sv[1] != 'v'))
		return false;
	sv.remove_prefix(2);
	if (sv.empty())
		return false;
	if (sv[0] == '0' && sv.size() != 1) // leading zero not allowed
		return false;
	uint32 v = 0;
	while (!sv.empty())
	{
		uint8 c = sv[0];
		sv.remove_prefix(1);
		v *= 10;
		if (c >= '0' && c <= '9')
			v += (uint32)(c - '0');
		else
		{
			v = 0xFFFFFFFF;
			break;
		}
	}
	if (v > 0xFFFF)
		return false;
	titleIdOut = parsedId;
	titleVersionOut = v;
	return true;
}

bool TitleInfo::DetectFormat(const fs::path& path, fs::path& pathOut, TitleDataFormat& formatOut)
{
	std::error_code ec;
	if (path.has_extension() && fs::is_regular_file(path, ec))
	{
		std::string filenameStr = _pathToUtf8(path.filename());
		if (boost::iends_with(filenameStr, ".rpx"))
		{
			// is in code folder?
			fs::path parentPath = path.parent_path();
			if (boost::iequals(_pathToUtf8(parentPath.filename()), "code"))
			{
				parentPath = parentPath.parent_path();
				// next to content and meta?
				std::error_code ec;
				if (fs::exists(parentPath / "content", ec) && fs::exists(parentPath / "meta", ec))
				{
					formatOut = TitleDataFormat::HOST_FS;
					pathOut = parentPath;
					return true;
				}
			}
		}
		else if (boost::iends_with(filenameStr, ".wud") ||
				 boost::iends_with(filenameStr, ".wux") ||
				 boost::iends_with(filenameStr, ".iso"))
		{
			formatOut = TitleDataFormat::WUD;
			pathOut = path;
			return true;
		}
		else if (boost::iequals(filenameStr, "title.tmd"))
		{
			formatOut = TitleDataFormat::NUS;
			pathOut = path;
			return true;
		}
		else if (boost::iends_with(filenameStr, ".wua"))
		{
			formatOut = TitleDataFormat::WIIU_ARCHIVE;
			pathOut = path;
			// a Wii U archive file can contain multiple titles but TitleInfo only maps to one
			// we use the first base title that we find. This is the most intuitive behavior when someone launches "game.wua"
			ZArchiveReader* zar = ZArchiveReader::OpenFromFile(path);
			if (!zar)
				return false;
			ZArchiveNodeHandle rootDir = zar->LookUp("", false, true);
			bool foundBase = false;
			for (uint32 i = 0; i < zar->GetDirEntryCount(rootDir); i++)
			{
				ZArchiveReader::DirEntry dirEntry;
				if (!zar->GetDirEntry(rootDir, i, dirEntry))
					continue;
				if (!dirEntry.isDirectory)
					continue;
				TitleId parsedId;
				uint16 parsedVersion;
				if (!TitleInfo::ParseWuaTitleFolderName(dirEntry.name, parsedId, parsedVersion))
					continue;
				TitleIdParser tip(parsedId);
				TitleIdParser::TITLE_TYPE tt = tip.GetType();
				if (tt == TitleIdParser::TITLE_TYPE::BASE_TITLE || tt == TitleIdParser::TITLE_TYPE::BASE_TITLE_DEMO ||
					tt == TitleIdParser::TITLE_TYPE::SYSTEM_TITLE || tt == TitleIdParser::TITLE_TYPE::SYSTEM_OVERLAY_TITLE)
				{
					m_subPath = dirEntry.name;
					foundBase = true;
					break;
				}
			}
			delete zar;
			return foundBase;
		}
		else if (boost::iends_with(filenameStr, ".wuhb"))
		{
			std::unique_ptr<WUHBReader> reader{WUHBReader::FromPath(path)};
			if(reader)
			{
				formatOut = TitleDataFormat::WUHB;
				pathOut = path;
				return true;
			}
		}
		// note: Since a Wii U archive file (.wua) contains multiple titles we shouldn't auto-detect them here
		// instead TitleInfo has a second constructor which takes a subpath
		// unable to determine type by extension, check contents
		CafeTitleFileType fileType = DetermineCafeSystemFileType(path);
		if (fileType == CafeTitleFileType::WUD ||
			fileType == CafeTitleFileType::WUX)
		{
			formatOut = TitleDataFormat::WUD;
			pathOut = path;
			return true;
		}
	}
	else
	{
		// does it point to the root folder of a title?
		std::error_code ec;
		if (fs::exists(path / "content", ec) && fs::exists(path / "meta", ec) && fs::exists(path / "code", ec))
		{
			formatOut = TitleDataFormat::HOST_FS;
			pathOut = path;
			return true;
		}
	}
	SetInvalidReason(InvalidReason::UNKNOWN_FORMAT);
	return false;
}

bool TitleInfo::IsValid() const
{
	return m_isValid;
}

fs::path TitleInfo::GetPath() const
{
	if (!m_isValid)
	{
		cemu_assert_suspicious();
		return {};
	}
	return m_fullPath;
}

void TitleInfo::CalcUID()
{
	cemu_assert_debug(m_isValid);
	if (!m_isValid)
	{
		m_uid = 0;
		return;
	}
	// get absolute normalized path
	fs::path normalizedPath;
	if (m_fullPath.is_relative())
	{
		normalizedPath = ActiveSettings::GetUserDataPath();
		normalizedPath /= m_fullPath;
	}
	else
		normalizedPath = m_fullPath;
	normalizedPath = normalizedPath.lexically_normal();
	uint64 h = fs::hash_value(normalizedPath);
	// for WUA files also hash the subpath
	if (m_titleFormat == TitleDataFormat::WIIU_ARCHIVE)
	{
		uint64 subHash = std::hash<std::string_view>{}(m_subPath);
		h += subHash;
	}
	m_uid = h;
}

uint64 TitleInfo::GetUID()
{
	cemu_assert_debug(m_isValid);
	return m_uid;
}

void TitleInfo::SetInvalidReason(InvalidReason reason)
{
	if(m_invalidReason == InvalidReason::NONE)
		m_invalidReason = reason; // only update reason when it hasn't been set before
}

std::mutex sZArchivePoolMtx;
std::map<fs::path, std::pair<uint32, ZArchiveReader*>> sZArchivePool;

ZArchiveReader* _ZArchivePool_AcquireInstance(const fs::path& path)
{
	std::unique_lock _lock(sZArchivePoolMtx);
	auto it = sZArchivePool.find(path);
	if (it != sZArchivePool.end())
	{
		it->second.first++; // increment ref count
		return it->second.second;
	}
	_lock.unlock();
	// opening wua files can be expensive, so we do it outside of the lock
	ZArchiveReader* zar = ZArchiveReader::OpenFromFile(path);
	if (!zar)
		return nullptr;
	_lock.lock();
	// check if another instance was allocated in the meantime
	it = sZArchivePool.find(path);
	if (it != sZArchivePool.end())
	{
		delete zar;
		it->second.first++; // increment ref count
		return it->second.second;
	}
	sZArchivePool.emplace(std::piecewise_construct,
		std::forward_as_tuple(path),
		std::forward_as_tuple(1, zar)
	);
	return zar;
}

void _ZArchivePool_ReleaseInstance(const fs::path& path, ZArchiveReader* zar)
{
	std::unique_lock _lock(sZArchivePoolMtx);
	auto it = sZArchivePool.find(path);
	cemu_assert(it != sZArchivePool.end());
	cemu_assert(it->second.second == zar);
	it->second.first--; // decrement ref count
	if (it->second.first == 0)
	{
		delete it->second.second;
		sZArchivePool.erase(it);
	}
}

bool TitleInfo::Mount(std::string_view virtualPath, std::string_view subfolder, sint32 mountPriority)
{
	cemu_assert_debug(subfolder.empty() || (subfolder.front() != '/' || subfolder.front() != '\\')); // only relative subfolder allowed

	cemu_assert(m_isValid);
	if (m_titleFormat == TitleDataFormat::HOST_FS)
	{
		fs::path hostFSPath = m_fullPath;
		hostFSPath.append(subfolder);
		bool r = FSCDeviceHostFS_Mount(std::string(virtualPath).c_str(), _pathToUtf8(hostFSPath), mountPriority);
		cemu_assert_debug(r);
		if (!r)
		{
			cemuLog_log(LogType::Force, "Failed to mount {} to {}", virtualPath, subfolder);
			SetInvalidReason(InvalidReason::BAD_PATH_OR_INACCESSIBLE);
			return false;
		}
	}
	else if (m_titleFormat == TitleDataFormat::WUD || m_titleFormat == TitleDataFormat::NUS)
	{
		FSTVolume::ErrorCode fstError;
		if (m_mountpoints.empty())
		{
			cemu_assert_debug(!m_wudVolume);
			if(m_titleFormat == TitleDataFormat::WUD)
				m_wudVolume = FSTVolume::OpenFromDiscImage(m_fullPath, &fstError); // open wud/wux
			else
				m_wudVolume = FSTVolume::OpenFromContentFolder(m_fullPath.parent_path(), &fstError); // open from .app files directory, the path points to /title.tmd
		}
		if (!m_wudVolume)
		{
			if (fstError == FSTVolume::ErrorCode::DISC_KEY_MISSING)
				SetInvalidReason(InvalidReason::NO_DISC_KEY);
			else if (fstError == FSTVolume::ErrorCode::TITLE_TIK_MISSING)
				SetInvalidReason(InvalidReason::NO_TITLE_TIK);
			return false;
		}
		bool r = FSCDeviceWUD_Mount(virtualPath, subfolder, m_wudVolume, mountPriority);
		cemu_assert_debug(r);
		if (!r)
		{
			cemuLog_log(LogType::Force, "Failed to mount {} to {}", virtualPath, subfolder);
			delete m_wudVolume;
			return false;
		}
	}
	else if (m_titleFormat == TitleDataFormat::WIIU_ARCHIVE)
	{
		if (!m_zarchive)
		{
			m_zarchive = _ZArchivePool_AcquireInstance(m_fullPath);
			if (!m_zarchive)
				return false;
		}
		bool r = FSCDeviceWUA_Mount(virtualPath, std::string(m_subPath).append("/").append(subfolder), m_zarchive, mountPriority);
		if (!r)
		{
			cemuLog_log(LogType::Force, "Failed to mount {} to {}", virtualPath, subfolder);
			_ZArchivePool_ReleaseInstance(m_fullPath, m_zarchive);
			return false;
		}
	}
	else if (m_titleFormat == TitleDataFormat::WUHB)
	{
		if (!m_wuhbreader)
		{
			m_wuhbreader = WUHBReader::FromPath(m_fullPath);
			if (!m_wuhbreader)
				return false;
		}
		bool r = FSCDeviceWUHB_Mount(virtualPath, subfolder, m_wuhbreader, mountPriority);
		if (!r)
		{
			cemuLog_log(LogType::Force, "Failed to mount {} to {}", virtualPath, subfolder);
			delete m_wuhbreader;
			m_wuhbreader = nullptr;
			return false;
		}
	}
	else
	{
		cemu_assert_unimplemented();
	}
	m_mountpoints.emplace_back(mountPriority, virtualPath);
	return true;
}

void TitleInfo::Unmount(std::string_view virtualPath)
{
	for (auto& itr : m_mountpoints)
	{
		if (!boost::equals(itr.second, virtualPath))
			continue;
		fsc_unmount(itr.second.c_str(), itr.first);
		std::erase(m_mountpoints, itr);
		// if the last mount point got unmounted, close any open devices
		if (m_mountpoints.empty())
		{
			if (m_wudVolume)
			{
				cemu_assert_debug(m_titleFormat == TitleDataFormat::WUD || m_titleFormat == TitleDataFormat::NUS);
				delete m_wudVolume;
				m_wudVolume = nullptr;
			}
            if (m_zarchive)
            {
                _ZArchivePool_ReleaseInstance(m_fullPath, m_zarchive);
                if (m_mountpoints.empty())
                    m_zarchive = nullptr;
            }
			if (m_wuhbreader)
			{
				cemu_assert_debug(m_titleFormat == TitleDataFormat::WUHB);
				delete m_wuhbreader;
				m_wuhbreader = nullptr;
			}
		}
		return;
	}
	cemu_assert_suspicious(); // unmount on unknown path
}

void TitleInfo::UnmountAll()
{
	while (!m_mountpoints.empty())
		Unmount(m_mountpoints.front().second);
}

std::atomic_uint64_t sTempMountingPathCounter = 1;

std::string TitleInfo::GetUniqueTempMountingPath()
{
	uint64_t v = sTempMountingPathCounter.fetch_add(1);
	return fmt::format("/internal/tempMount{:016x}/", v);
}

bool TitleInfo::ParseXmlInfo()
{
	cemu_assert(m_isValid);
	if (m_hasParsedXmlFiles)
		return m_isValid;
	m_hasParsedXmlFiles = true;

	std::string mountPath = GetUniqueTempMountingPath();
	bool r = Mount(mountPath, "", FSC_PRIORITY_BASE);
	if (!r)
		return false;
	// meta/meta.xml
	auto xmlData = fsc_extractFile(fmt::format("{}meta/meta.xml", mountPath).c_str());
	if(xmlData)
		m_parsedMetaXml = ParsedMetaXml::Parse(xmlData->data(), xmlData->size());

	if(!m_parsedMetaXml)
	{
		// meta/meta.ini (WUHB)
		auto iniData = fsc_extractFile(fmt::format("{}meta/meta.ini", mountPath).c_str());
		if (iniData)
			m_parsedMetaXml = ParseAromaIni(*iniData);
		if(m_parsedMetaXml)
		{
			m_parsedCosXml = new ParsedCosXml{.argstr = "root.rpx"};
			m_parsedAppXml = new ParsedAppXml{m_parsedMetaXml->m_title_id, 0, 0, 0, 0};
		}
	}

	// code/app.xml
	xmlData = fsc_extractFile(fmt::format("{}code/app.xml", mountPath).c_str());
	if(xmlData)
		ParseAppXml(*xmlData);
	// code/cos.xml
	xmlData = fsc_extractFile(fmt::format("{}code/cos.xml", mountPath).c_str());
	if (xmlData)
		m_parsedCosXml = ParsedCosXml::Parse(xmlData->data(), xmlData->size());

	Unmount(mountPath);

	// some system titles dont have a meta.xml file
	bool allowMissingMetaXml = false;
	if(m_parsedAppXml && this->IsSystemDataTitle())
	{
		allowMissingMetaXml = true;
	}

	if ((allowMissingMetaXml == false && !m_parsedMetaXml) || !m_parsedAppXml || !m_parsedCosXml)
	{
		bool hasAnyXml = m_parsedMetaXml || m_parsedAppXml || m_parsedCosXml;
		if (hasAnyXml)
			cemuLog_log(LogType::Force, "Title has missing meta .xml files. Title path: {}", _pathToUtf8(m_fullPath));
		delete m_parsedMetaXml;
		delete m_parsedAppXml;
		delete m_parsedCosXml;
		m_parsedMetaXml = nullptr;
		m_parsedAppXml = nullptr;
		m_parsedCosXml = nullptr;
		m_isValid = false;
		SetInvalidReason(InvalidReason::MISSING_XML_FILES);
		return false;
	}
	m_isValid = true;
	return true;
}

ParsedMetaXml* TitleInfo::ParseAromaIni(std::span<unsigned char> content)
{
	IniParser parser{content};
	while (parser.NextSection() && parser.GetCurrentSectionName() != "menu")
		continue;
	if (parser.GetCurrentSectionName() != "menu")
		return nullptr;

	auto parsed = std::make_unique<ParsedMetaXml>();

	const auto author = parser.FindOption("author");
	if (author)
		parsed->m_publisher[(size_t)CafeConsoleLanguage::EN] = *author;

	const auto longName = parser.FindOption("longname");
	if (longName)
		parsed->m_long_name[(size_t)CafeConsoleLanguage::EN] = *longName;

	const auto shortName = parser.FindOption("shortname");
	if (shortName)
		parsed->m_short_name[(size_t)CafeConsoleLanguage::EN] = *shortName;

	auto checksumInput = std::string{*author}.append(*longName).append(*shortName);
	parsed->m_title_id = (0x0005000Full<<32) | crc32_calc(checksumInput.data(), checksumInput.length());

	return parsed.release();
}

bool TitleInfo::ParseAppXml(std::vector<uint8>& appXmlData)
{
	pugi::xml_document app_doc;
	if (!app_doc.load_buffer_inplace(appXmlData.data(), appXmlData.size()))
		return false;

	const auto root = app_doc.child("app");
	if (!root)
		return false;

	m_parsedAppXml = new ParsedAppXml();

	for (const auto& child : root.children())
	{
		std::string_view name = child.name();
		if (name == "title_version")
			m_parsedAppXml->title_version = (uint16)std::stoull(child.text().as_string(), nullptr, 16);
		else if (name == "title_id")
			m_parsedAppXml->title_id = std::stoull(child.text().as_string(), nullptr, 16);
		else if (name == "app_type")
			m_parsedAppXml->app_type = (uint32)std::stoull(child.text().as_string(), nullptr, 16);
		else if (name == "group_id")
			m_parsedAppXml->group_id = (uint32)std::stoull(child.text().as_string(), nullptr, 16);
		else if (name == "sdk_version")
			m_parsedAppXml->sdk_version = (uint32)std::stoull(child.text().as_string(), nullptr, 10);
	}
	return true;
}

TitleId TitleInfo::GetAppTitleId() const
{
	cemu_assert_debug(m_isValid);
	if (m_parsedAppXml)
		return m_parsedAppXml->title_id;
	if (m_cachedInfo)
		return m_cachedInfo->titleId;
	cemu_assert_suspicious();
	return 0;
}

uint16 TitleInfo::GetAppTitleVersion() const
{
	cemu_assert_debug(m_isValid);
	if (m_parsedAppXml)
		return m_parsedAppXml->title_version;
	if (m_cachedInfo)
		return m_cachedInfo->titleVersion;
	cemu_assert_suspicious();
	return 0;
}

uint32 TitleInfo::GetAppSDKVersion() const
{
	cemu_assert_debug(m_isValid);
	if (m_parsedAppXml)
		return m_parsedAppXml->sdk_version;
	if (m_cachedInfo)
		return m_cachedInfo->sdkVersion;
	cemu_assert_suspicious();
	return 0;
}

uint32 TitleInfo::GetAppGroup() const
{
	cemu_assert_debug(m_isValid);
	if (m_parsedAppXml)
		return m_parsedAppXml->group_id;
	if (m_cachedInfo)
		return m_cachedInfo->group_id;
	cemu_assert_suspicious();
	return 0;
}

uint32 TitleInfo::GetAppType() const
{
	cemu_assert_debug(m_isValid);
	if (m_parsedAppXml)
		return m_parsedAppXml->app_type;
	if (m_cachedInfo)
		return m_cachedInfo->app_type;
	cemu_assert_suspicious();
	return 0;
}

TitleIdParser::TITLE_TYPE TitleInfo::GetTitleType()
{
	TitleIdParser tip(GetAppTitleId());
	return tip.GetType();
}

std::string TitleInfo::GetMetaTitleName() const
{
	cemu_assert_debug(m_isValid);
	if (m_parsedMetaXml)
	{
		std::string titleNameCfgLanguage;
		titleNameCfgLanguage = m_parsedMetaXml->GetLongName(GetConfig().console_language);
		if (titleNameCfgLanguage.empty()) //Get English Title
			titleNameCfgLanguage = m_parsedMetaXml->GetLongName(CafeConsoleLanguage::EN);
		if (titleNameCfgLanguage.empty()) //Unknown Title
			titleNameCfgLanguage = "Unknown Title";
		return titleNameCfgLanguage;
	}
	if (m_cachedInfo)
		return m_cachedInfo->titleName;
	return "";
}

CafeConsoleRegion TitleInfo::GetMetaRegion() const
{
	cemu_assert_debug(m_isValid);
	if (m_parsedMetaXml)
		return m_parsedMetaXml->GetRegion();
	if (m_cachedInfo)
		return m_cachedInfo->region;
	return CafeConsoleRegion::JPN;
}

uint32 TitleInfo::GetOlvAccesskey() const
{
	cemu_assert_debug(m_isValid);
	if (m_parsedMetaXml)
		return m_parsedMetaXml->GetOlvAccesskey();

	cemu_assert_suspicious();
	return -1;
}

std::string TitleInfo::GetArgStr() const
{
	cemu_assert_debug(m_parsedCosXml);
	if (!m_parsedCosXml)
		return "";
	return m_parsedCosXml->argstr;
}

std::string TitleInfo::GetPrintPath() const
{
	if (!m_isValid)
		return "invalid";
	std::string tmp;
	tmp.append(_pathToUtf8(m_fullPath));
	switch (m_titleFormat)
	{
	case TitleDataFormat::HOST_FS:
		tmp.append(" [Folder]");
		break;
	case TitleDataFormat::WUD:
		tmp.append(" [WUD]");
		break;
	case TitleDataFormat::NUS:
		tmp.append(" [NUS]");
		break;
	case TitleDataFormat::WIIU_ARCHIVE:
		tmp.append(" [WUA]");
		break;
	case TitleDataFormat::WUHB:
		tmp.append(" [WUHB]");
		break;
	default:
		break;
	}
	if (m_titleFormat == TitleDataFormat::WIIU_ARCHIVE)
		tmp.append(fmt::format(" [{}]", m_subPath));
	return tmp;
}

std::string TitleInfo::GetInstallPath() const
{
	TitleId titleId = GetAppTitleId();
	TitleIdParser tip(titleId);
	std::string tmp;
	if (tip.IsSystemTitle())
		tmp = fmt::format("sys/title/{:08x}/{:08x}", GetTitleIdHigh(titleId), GetTitleIdLow(titleId));
    else
		tmp = fmt::format("usr/title/{:08x}/{:08x}", GetTitleIdHigh(titleId), GetTitleIdLow(titleId));
	return tmp;
}

ParsedCosXml* ParsedCosXml::Parse(uint8* xmlData, size_t xmlLen)
{
	pugi::xml_document app_doc;
	if (!app_doc.load_buffer_inplace(xmlData, xmlLen))
		return nullptr;

	const auto root = app_doc.child("app");
	if (!root)
		return nullptr;

	ParsedCosXml* parsedCos = new ParsedCosXml();

	auto node = root.child("argstr");
	if (node)
		parsedCos->argstr = node.text().as_string();

	// parse permissions
	auto permissionsNode = root.child("permissions");
	for(uint32 permissionIndex = 0; permissionIndex < 19; ++permissionIndex)
	{
		std::string permissionName = fmt::format("p{}", permissionIndex);
		auto permissionNode = permissionsNode.child(permissionName.c_str());
		if (!permissionNode)
			break;
		parsedCos->permissions[permissionIndex].group = static_cast<CosCapabilityGroup>(ConvertString<uint32>(permissionNode.child("group").text().as_string(), 10));
		parsedCos->permissions[permissionIndex].mask = static_cast<CosCapabilityBits>(ConvertString<uint64>(permissionNode.child("mask").text().as_string(), 16));
	}

	return parsedCos;
}