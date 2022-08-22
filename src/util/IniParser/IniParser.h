#pragma once

#include <vector>
#include <span>
#include <string>
#include <optional>

class IniParser
{
private:
	class IniSection
	{
	public:
		IniSection(std::string_view sectionName, size_t lineNumber) : m_sectionName(sectionName), m_lineNumber(lineNumber) {}
		std::string_view m_sectionName;
		size_t m_lineNumber;
		std::vector<std::pair<std::string_view, std::string_view>> m_optionPairs;
	};

public:
	IniParser(std::span<char> iniContents, std::string_view name = {});
	IniParser(std::span<unsigned char> iniContents, std::string_view name = {}) : IniParser(std::span<char>((char*)iniContents.data(), iniContents.size()), name) {};

	// section and option iterating
	bool NextSection();
	std::string_view GetCurrentSectionName();
	size_t GetCurrentSectionLineNumber();
	std::optional<std::string_view> FindOption(std::string_view optionName);
	std::span<std::pair<std::string_view, std::string_view>> GetAllOptions();

private:
	// parsing
	bool parse();
	bool ReadNextLine(std::string_view& lineString);
	void TrimWhitespaces(std::string_view& str);
	void StartSection(std::string_view sectionName, size_t lineNumber);
	void PrintWarning(int lineNumber, std::string_view msg, std::string_view lineView);

	std::vector<char> m_iniFileData;
	std::string m_name;
	bool m_isValid{ false };
	size_t m_parseOffset{ 0 };
	std::vector<IniSection> m_sectionList;
	size_t m_currentSectionIndex{std::numeric_limits<size_t>::max()};
};