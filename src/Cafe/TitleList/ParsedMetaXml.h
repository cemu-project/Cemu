#pragma once

#include <pugixml.hpp>
#include "config/CemuConfig.h"

struct ParsedMetaXml
{
	uint32 m_version;
	std::string m_product_code;
	std::string m_company_code;
	std::string m_content_platform;
	uint64 m_title_id;
	CafeConsoleRegion m_region;

	std::array<std::string, 12> m_long_name;
	std::array<std::string, 12> m_short_name;
	std::array<std::string, 12> m_publisher;

	uint32 m_olv_accesskey;

	std::string GetShortName(CafeConsoleLanguage languageId) const
	{
		return m_short_name[(size_t)languageId].empty() ? m_short_name[(size_t)CafeConsoleLanguage::EN] : m_short_name[(size_t)languageId];
	}

	std::string GetLongName(CafeConsoleLanguage languageId) const
	{
		return m_long_name[(size_t)languageId].empty() ? m_long_name[(size_t)CafeConsoleLanguage::EN] : m_long_name[(size_t)languageId];
	}

	TitleId GetTitleId() const
	{
		return m_title_id;
	}

	uint16 GetTitleVersion() const
	{
		return (uint16)m_version;
	}

	CafeConsoleRegion GetRegion() const
	{
		return m_region;
	}

	std::string GetProductCode() const
	{
		return m_product_code;
	}

	std::string GetCompanyCode() const
	{
		return m_company_code;
	}

	uint32 GetOlvAccesskey() const
	{
		return m_olv_accesskey;
	}

	static ParsedMetaXml* Parse(uint8* xmlData, size_t xmlSize)
	{
		if (xmlSize == 0)
			return nullptr;
		pugi::xml_document meta_doc;
		if (!meta_doc.load_buffer_inplace(xmlData, xmlSize))
			return nullptr;

		const auto root = meta_doc.child("menu");
		if (!root)
			return nullptr;

		ParsedMetaXml* parsedMetaXml = new ParsedMetaXml();

		for (const auto& child : root.children())
		{
			std::string_view name = child.name();
			if (name == "title_version")
				parsedMetaXml->m_version = child.text().as_uint();
			else if (name == "product_code")
				parsedMetaXml->m_product_code = child.text().as_string();
			else if (name == "company_code")
				parsedMetaXml->m_company_code = child.text().as_string();
			else if (name == "content_platform")
				parsedMetaXml->m_content_platform = child.text().as_string();
			else if (name == "title_id")
				parsedMetaXml->m_title_id = std::stoull(child.text().as_string(), nullptr, 16);
			else if (name == "region")
				parsedMetaXml->m_region = (CafeConsoleRegion)child.text().as_uint();
			else if (boost::starts_with(name, "longname_"))
			{
				const sint32 index = GetLanguageIndex(name.substr(std::size("longname_") - 1));
				if (index != -1){
					std::string longname = child.text().as_string();
					std::replace_if(longname.begin(), longname.end(), [](char c) { return c == '\r' || c == '\n';}, ' ');
					parsedMetaXml->m_long_name[index] = longname;
				}
			}
			else if (boost::starts_with(name, L"shortname_"))
			{
				const sint32 index = GetLanguageIndex(name.substr(std::size("shortname_") - 1));
				if (index != -1)
					parsedMetaXml->m_short_name[index] = child.text().as_string();
			}
			else if (boost::starts_with(name, L"publisher_"))
			{
				const sint32 index = GetLanguageIndex(name.substr(std::size("publisher_") - 1));
				if (index != -1)
					parsedMetaXml->m_publisher[index] = child.text().as_string();
			}
			else if (boost::starts_with(name, L"olv_accesskey"))
				parsedMetaXml->m_olv_accesskey = child.text().as_uint(-1);
		}
		if (parsedMetaXml->m_title_id == 0)
		{
			// not valid
			delete parsedMetaXml;
			return nullptr;
		}
		return parsedMetaXml;
	}

private:
	static sint32 GetLanguageIndex(std::string_view language) // move to NCrypto ?
	{
		if (language == "ja")
			return (sint32)CafeConsoleLanguage::JA;
		else if (language == "en")
			return (sint32)CafeConsoleLanguage::EN;
		else if (language == "fr")
			return (sint32)CafeConsoleLanguage::FR;
		else if (language == "de")
			return (sint32)CafeConsoleLanguage::DE;
		else if (language == "it")
			return (sint32)CafeConsoleLanguage::IT;
		else if (language == "es")
			return (sint32)CafeConsoleLanguage::ES;
		else if (language == "zhs")
			return (sint32)CafeConsoleLanguage::ZH;
		else if (language == "ko")
			return (sint32)CafeConsoleLanguage::KO;
		else if (language == "nl")
			return (sint32)CafeConsoleLanguage::NL;
		else if (language == "pt")
			return (sint32)CafeConsoleLanguage::PT;
		else if (language == "ru")
			return (sint32)CafeConsoleLanguage::RU;
		else if (language == "zht")
			return (sint32)CafeConsoleLanguage::TW; // if return ZH here, xxx_zht values may cover xxx_zh values in function Parse()
		return -1;
	}
};
