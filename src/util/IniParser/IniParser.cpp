#include "util/IniParser/IniParser.h"

IniParser::IniParser(std::span<char> iniContents, std::string_view name) : m_name(name)
{
	// we dont support utf8 but still skip the byte order mark in case the user saved the document with the wrong encoding
	if (iniContents.size() >= 3 && (uint8)iniContents[0] == 0xEF && (uint8)iniContents[1] == 0xBB && (uint8)iniContents[2] == 0xBF)
		iniContents = iniContents.subspan(3);

	m_iniFileData.assign(iniContents.begin(), iniContents.end());
	m_isValid = parse();
}

bool IniParser::ReadNextLine(std::string_view& lineString)
{
	if (m_parseOffset >= m_iniFileData.size())
		return false;
	// skip \r and \n
	for (; m_parseOffset < m_iniFileData.size(); m_parseOffset++)
	{
		char c = m_iniFileData[m_parseOffset];
		if (c == '\r' || c == '\n')
			continue;
		break;
	}
	if (m_parseOffset >= m_iniFileData.size())
		return false;
	size_t lineStart = m_parseOffset;
	// parse until end of line/file
	for (; m_parseOffset < m_iniFileData.size(); m_parseOffset++)
	{
		char c = m_iniFileData[m_parseOffset];
		if (c == '\r' || c == '\n')
			break;
	}
	size_t lineEnd = m_parseOffset;
	lineString = { m_iniFileData.data() + lineStart, lineEnd - lineStart };
	return true;
}

void IniParser::TrimWhitespaces(std::string_view& str)
{
	while (!str.empty())
	{
		char c = str[0];
		if (c != ' ' && c != '\t')
			break;
		str.remove_prefix(1);
	}
	while (!str.empty())
	{
		char c = str.back();
		if (c != ' ' && c != '\t')
			break;
		str.remove_suffix(1);
	}
}

bool IniParser::parse()
{
	sint32 lineNumber = 0;
	std::string_view lineView;
	while (ReadNextLine(lineView))
	{
		lineNumber++;
		// skip whitespaces
		while (!lineView.empty())
		{
			char c = lineView[0];
			if (c != ' ' && c != '\t')
				break;
			lineView.remove_prefix(1);
		}
		if (lineView.empty())
			continue;
		// cut off comments (starting with # or ;)
		bool isInQuote = false;
		for (size_t i = 0; i < lineView.size(); i++)
		{
			if (lineView[i] == '\"')
				isInQuote = !isInQuote;
			if ((lineView[i] == '#' || lineView[i] == ';') && !isInQuote)
			{
				lineView.remove_suffix(lineView.size() - i);
				break;
			}
		}
		if(lineView.empty())
			continue;
		// handle section headers
		if (lineView[0] == '[')
		{
			isInQuote = false;
			bool endsWithBracket = false;
			for (size_t i = 1; i < lineView.size(); i++)
			{
				if (lineView[i] == '\"')
					isInQuote = !isInQuote;
				if (lineView[i] == ']')
				{
					lineView.remove_suffix(lineView.size() - i);
					lineView.remove_prefix(1);
					endsWithBracket = true;
					break;
				}
			}
			if (!endsWithBracket)
				PrintWarning(lineNumber, "Section doesn't end with a ]", lineView);
			StartSection(lineView, lineNumber);
			continue;
		}
		// otherwise try to parse it as an option in the form name = value
		// find and split at = character
		std::string_view option_name;
		std::string_view option_value;
		bool invalidName = true;
		for (size_t i = 0; i < lineView.size(); i++)
		{
			if (lineView[i] == '=')
			{
				option_name = lineView.substr(0, i);
				option_value = lineView.substr(i+1);
				invalidName = false;
				break;
			}

		}
		if (invalidName)
		{
			TrimWhitespaces(lineView);
			if (!lineView.empty())
				PrintWarning(lineNumber, "Not a valid section header or name-value pair", lineView);
			continue;
		}
		// validate
		TrimWhitespaces(option_name);
		TrimWhitespaces(option_value);
		if (option_name.empty())
		{
			PrintWarning(lineNumber, "Empty option name is not allowed", lineView);
			continue;
		}
		bool invalidCharacter = false;
		for (auto& _c : option_name)
		{
			uint8 c = (uint8)_c;
			if (c == ']' || c == '[')
			{
				PrintWarning(lineNumber, "Option name may not contain [ or ]", lineView);
				invalidCharacter = true;
				break;
			}
			else if (c < 32 || c > 128 || c == ' ')
			{
				PrintWarning(lineNumber, "Option name may only contain ANSI characters and no spaces", lineView);
				invalidCharacter = true;
				break;
			}
		}
		if(invalidCharacter)
			continue;
		// remove quotes from value
		if (!option_value.empty() && option_value.front() == '\"')
		{
			option_value.remove_prefix(1);
			if (option_value.size() >= 2 && option_value.back() == '\"')
			{
				option_value.remove_suffix(1);
			}
			else
			{
				PrintWarning(lineNumber, "Option value starts with a quote character \" but does not end with one", lineView);
				continue;
			}
		}
		if (m_sectionList.empty())
		{
			// no current section
			PrintWarning(lineNumber, "Option defined without first defining a section", lineView);
			continue;
		}
		// convert name to lower case
		m_sectionList.back().m_optionPairs.emplace_back(option_name, option_value);
	}
	return true;
}

void IniParser::StartSection(std::string_view sectionName, size_t lineNumber)
{
	m_sectionList.emplace_back(sectionName, lineNumber);
}

bool IniParser::NextSection()
{
	if (m_currentSectionIndex == std::numeric_limits<size_t>::max())
	{
		m_currentSectionIndex = 0;
		return m_currentSectionIndex < m_sectionList.size();
	}
	if (m_currentSectionIndex >= m_sectionList.size())
		return false;
	m_currentSectionIndex++;
	return m_currentSectionIndex < m_sectionList.size();
}

std::string_view IniParser::GetCurrentSectionName()
{
	if (m_currentSectionIndex == std::numeric_limits<size_t>::max() || m_currentSectionIndex >= m_sectionList.size())
		return "";
	return m_sectionList[m_currentSectionIndex].m_sectionName;
}

size_t IniParser::GetCurrentSectionLineNumber()
{
	if (m_currentSectionIndex == std::numeric_limits<size_t>::max() || m_currentSectionIndex >= m_sectionList.size())
		return 0;
	return m_sectionList[m_currentSectionIndex].m_lineNumber;
}

std::optional<std::string_view> IniParser::FindOption(std::string_view optionName)
{
	if (m_currentSectionIndex == std::numeric_limits<size_t>::max() || m_currentSectionIndex >= m_sectionList.size())
		return std::nullopt;
	auto& optionPairsList = m_sectionList[m_currentSectionIndex].m_optionPairs;
	for (auto& itr : optionPairsList)
	{
		auto& itrOptionName = itr.first;
		// case insensitive ANSI string comparison
		if(itrOptionName.size() != optionName.size())
			continue;
		bool isMatch = true;
		for (size_t i = 0; i < itrOptionName.size(); i++)
		{
			char c0 = itrOptionName[i];
			char c1 = optionName[i];
			if (c0 >= 'A' && c0 <= 'Z')
				c0 -= ('A' - 'a');
			if (c1 >= 'A' && c1 <= 'Z')
				c1 -= ('A' - 'a');
			if (c0 != c1)
			{
				isMatch = false;
				break;
			}
		}
		if (!isMatch)
			continue;
		return itr.second;
	}
	return std::nullopt;
}

std::span<std::pair<std::string_view, std::string_view>> IniParser::GetAllOptions()
{
	if (m_currentSectionIndex == std::numeric_limits<size_t>::max() || m_currentSectionIndex >= m_sectionList.size())
		return {};
	return m_sectionList[m_currentSectionIndex].m_optionPairs;
}

void IniParser::PrintWarning(int lineNumber, std::string_view msg, std::string_view lineView)
{
	// INI logging is silenced
	// cemuLog_log(LogType::Force, "File: {} Line {}: {}", m_name, lineNumber, msg);
}