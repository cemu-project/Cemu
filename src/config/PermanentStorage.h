#pragma once

// disabled by config
class PSDisabledException : public std::runtime_error
{
public:
	PSDisabledException()
		: std::runtime_error("permanent storage is disabled by user") {}
};

class PermanentStorage
{
public:
	PermanentStorage();
	~PermanentStorage();

	void ClearAllFiles() const;
	// flags storage to be removed on destruction
	void RemoveStorage();

	void WriteStringToFile(std::string_view filename, std::string_view content);
	std::string ReadFile(std::string_view filename) noexcept;
	
private:
	fs::path m_storage_path;
	bool m_remove_storage = false;
};