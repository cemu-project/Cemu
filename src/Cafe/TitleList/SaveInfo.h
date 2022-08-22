#pragma once

#include "TitleId.h"
#include "ParsedMetaXml.h"

class SaveInfo
{
public:
	SaveInfo() {};
	SaveInfo(TitleId titleId);

	bool IsValid() const { return m_isValid; }

	TitleId GetTitleId() const { return m_titleId; }
	fs::path GetPath() const { return m_path; }

	// meta data
	bool ParseMetaData();
	ParsedMetaXml* GetMetaInfo() { return m_parsedMetaXml; }

private:
	static std::string GetStorageSubpathByTitleId(TitleId titleId);
	static fs::path GetSavePath(TitleId titleId);

	TitleId m_titleId;
	fs::path m_path;
	bool m_isValid{false};
	bool m_hasMetaLoaded{false};
	ParsedMetaXml* m_parsedMetaXml{nullptr};
};