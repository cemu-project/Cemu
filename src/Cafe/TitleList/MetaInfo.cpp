#include "MetaInfo.h"
#include "Cafe/Filesystem/fsc.h"

#include "pugixml.hpp"
#include "Cafe/Filesystem/FST/FST.h"

MetaInfo::MetaInfo()
{
	m_type = GameType::FSC;
	
	uint32 meta_size;
	const auto meta_data = ReadFSCFile("vol/meta/meta.xml", meta_size);
	if (meta_size == 0 || !meta_data)
		throw std::runtime_error("meta.xml missing");

	pugi::xml_document meta_doc;
	ParseMetaFile(meta_doc, meta_doc.load_buffer_inplace(meta_data.get(), meta_size));
}

MetaInfo::MetaInfo(const fs::path& filename)
{
	if (!fs::exists(filename))
		throw std::invalid_argument("filename doesn't exist");

	if (fs::is_directory(filename))
	{
		MetaInfo::ParseDirectory(filename);
		m_type = GameType::Directory;
		m_type_path = filename;
	}
	else
		MetaInfo::ParseFile(filename);
}

std::string MetaInfo::GetName(CafeConsoleLanguage language) const
{
	std::string long_name{ GetLongName(language) };
	const auto nl = long_name.find(L'\n');
	if (nl != std::string::npos)
		long_name.replace(nl, 1, " - ");

	return long_name;
}

const std::string& MetaInfo::GetLongName(CafeConsoleLanguage language) const
{
	return m_long_name[(int)language].empty() ? m_long_name[(int)CafeConsoleLanguage::EN] : m_long_name[(int)language];
}

const std::string& MetaInfo::GetShortName(CafeConsoleLanguage language) const
{
	return m_short_name[(int)language].empty() ? m_short_name[(int)CafeConsoleLanguage::EN] : m_short_name[(int)language];
}

const std::string& MetaInfo::GetPublisher(CafeConsoleLanguage language) const
{
	return m_publisher[(int)language].empty() ? m_publisher[(int)CafeConsoleLanguage::EN] : m_publisher[(int)language];
}

void MetaInfo::ParseDirectory(const fs::path& filename)
{
	const auto meta_dir = fs::path(filename).append(L"meta");
	if (!fs::exists(meta_dir) || !fs::is_directory(meta_dir))
		throw std::invalid_argument("meta directory missing");

	const auto meta_file = meta_dir / L"meta.xml";
	if (!fs::exists(meta_file) || !fs::is_regular_file(meta_file))
		throw std::invalid_argument("meta.xml missing");

	ParseMetaFile(meta_file.wstring());
}

bool MetaInfo::ParseFile(const fs::path& filename)
{
	if (filename.filename() != "meta.xml")
		return false;
	
	const auto base_dir = filename.parent_path().parent_path();

	ParseMetaFile(filename);
	m_type = GameType::Directory;
	m_type_path = base_dir;
	return true;
}

void MetaInfo::ParseMetaFile(const fs::path& meta_file)
{
	pugi::xml_document doc;
	const auto result = doc.load_file(meta_file.wstring().c_str());
	ParseMetaFile(doc, result);
}

void MetaInfo::ParseMetaFile(const pugi::xml_document& doc, const pugi::xml_parse_result& result)
{
	if (!result)
		throw std::invalid_argument(fmt::format("error when parsing the meta.xml: {}", result.description()));

	const auto root = doc.child("menu");
	if (!root)
		throw std::invalid_argument("meta.xml invalid");

	for (const auto& child : root.children())
	{
		std::string_view name = child.name();
		if (name == "title_version")
			m_version = child.text().as_uint();
		else if (name == "product_code")
			m_product_code = child.text().as_string();
		else if (name == "company_code")
			m_company_code = child.text().as_string();
		else if (name == "content_platform")
			m_content_platform = child.text().as_string();
		else if (name == "title_id")
			m_title_id = std::stoull(child.text().as_string(), nullptr, 16);
		else if (name == "region")
			m_region = (CafeConsoleRegion)child.text().as_uint();
		else if (boost::starts_with(name, "longname_"))
		{
			const sint32 index = GetLanguageIndex(name.substr(std::size("longname_") - 1));
			if (index != -1)
				m_long_name[index] = child.text().as_string();
		}
		else if (boost::starts_with(name, L"shortname_"))
		{
			const sint32 index = GetLanguageIndex(name.substr(std::size("shortname_") - 1));
			if (index != -1)
				m_short_name[index] = child.text().as_string();
		}
		else if (boost::starts_with(name, L"publisher_"))
		{
			const sint32 index = GetLanguageIndex(name.substr(std::size("publisher_") - 1));
			if (index != -1)
				m_publisher[index] = child.text().as_string();
		}
	}
}

std::unique_ptr<uint8[]> MetaInfo::GetIcon(uint32& size) const
{
	size = 0;
	switch (m_type)
	{
	case GameType::FSC:
		return ReadFSCFile("vol/meta/iconTex.tga", size);
	case GameType::Directory:
	{
		cemu_assert_debug(!m_type_path.empty());
		const auto icon = fs::path(m_type_path).append(L"meta").append(L"iconTex.tga");
		std::ifstream file(icon, std::ios::binary | std::ios::ate);
		if (file.is_open())
		{
			size = file.tellg();
			if (size > 0)
			{
				file.seekg(0, std::ios::beg);
				auto result = std::make_unique<uint8[]>(size);
				file.read((char*)result.get(), size);
				return result;
			}
		}
		return nullptr;
	}
	case GameType::Image:
	{
		cemu_assert_debug(!m_type_path.empty());
		FSTVolume* volume = FSTVolume::OpenFromDiscImage(m_type_path);
		if (volume)
		{
			auto result = ReadVirtualFile(volume, "meta/iconTex.tga", size);
			delete volume;
			return result;
		}
		return nullptr;
	}
	default:
		UNREACHABLE;
	}
	return nullptr;
}
