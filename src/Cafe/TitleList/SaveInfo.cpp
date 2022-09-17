#include "SaveInfo.h"
#include "config/ActiveSettings.h"
#include "Common/FileStream.h"
#include "ParsedMetaXml.h"

SaveInfo::SaveInfo(TitleId titleId) : m_titleId(titleId)
{
	m_path = GetSavePath(titleId);
	std::error_code ec;
	m_isValid = fs::is_directory(m_path, ec);
}

std::string SaveInfo::GetStorageSubpathByTitleId(TitleId titleId)
{
	// usr/save/<titleIdHigh>/<titleIdLow>/
	return fmt::format("usr/save/{:08x}/{:08x}", ((uint64)titleId) >> 32, (uint64)titleId & 0xFFFFFFFF);
}

fs::path SaveInfo::GetSavePath(TitleId titleId)
{
	return ActiveSettings::GetMlcPath(GetStorageSubpathByTitleId(titleId));
}

bool SaveInfo::ParseMetaData()
{
	if (m_hasMetaLoaded)
		return m_parsedMetaXml != nullptr;
	m_hasMetaLoaded = true;
	auto xmlData = FileStream::LoadIntoMemory(m_path / "meta/meta.xml");
	if (!xmlData)
		return false;
	m_parsedMetaXml = ParsedMetaXml::Parse(xmlData->data(), xmlData->size());
	return m_parsedMetaXml != nullptr;
}