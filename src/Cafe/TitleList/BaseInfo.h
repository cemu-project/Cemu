#pragma once

namespace pugi
{
	struct xml_parse_result;
	class xml_document;
}

class BaseInfo
{
public:
	enum class GameType
	{
		FSC, // using fsc API
		Directory, // rpx/meta
		Image, // wud/wux
	};
	
	virtual ~BaseInfo() = default;

	[[nodiscard]] const fs::path& GetPath() const { return m_type_path; }
	[[nodiscard]] GameType GetGameType() const { return m_type; }
	
protected:
	
	GameType m_type;
	fs::path m_type_path; // empty / base dir / wud path

	virtual void ParseDirectory(const fs::path& filename) = 0;
	virtual bool ParseFile(const fs::path& filename) = 0;

	
	[[nodiscard]] std::unique_ptr<uint8[]> ReadFSCFile(std::string_view filename, uint32& size) const;
	[[nodiscard]] std::unique_ptr<uint8[]> ReadVirtualFile(class FSTVolume* volume, std::string_view filename, uint32& size) const;

	[[nodiscard]] static sint32 GetLanguageIndex(std::string_view language);
};