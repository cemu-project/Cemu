#include "PermanentStorage.h"
#include "config/CemuConfig.h"
#include "util/helpers/SystemException.h"

PermanentStorage::PermanentStorage()
{
	if (!GetConfig().permanent_storage)
		throw PSDisabledException();
	
	const char* appdata = std::getenv("LOCALAPPDATA");
	if (!appdata)
		throw std::runtime_error("can't get LOCALAPPDATA");
	m_storage_path = appdata;
	m_storage_path /= "Cemu";
	
	fs::create_directories(m_storage_path);
}

PermanentStorage::~PermanentStorage()
{
	if (m_remove_storage)
	{
		std::error_code ec;
		fs::remove_all(m_storage_path, ec);
		if (ec)
		{
			SystemException ex(ec);
			forceLog_printf("can't remove permanent storage: %s", ex.what());
		}
	}
}

void PermanentStorage::ClearAllFiles() const
{
	fs::remove_all(m_storage_path);
	fs::create_directories(m_storage_path);
}

void PermanentStorage::RemoveStorage()
{
	m_remove_storage = true;
}

void PermanentStorage::WriteStringToFile(std::string_view filename, std::string_view content)
{
	const auto name = m_storage_path.append(filename.data(), filename.data() + filename.size());
	std::ofstream file(name.string());
	file.write(content.data(), (uint32_t)content.size());
}

std::string PermanentStorage::ReadFile(std::string_view filename) noexcept
{
	try
	{
		const auto name = m_storage_path.append(filename.data(), filename.data() + filename.size());
		std::ifstream file(name, std::ios::in | std::ios::ate);
		if (!file.is_open())
			return {};

		const auto end = file.tellg();
		file.seekg(0, std::ios::beg);
		const auto file_size = end - file.tellg();
		if (file_size == 0)
			return {};

		std::string result;
		result.resize(file_size);
		file.read(result.data(), file_size);
		return result;
	}
	catch (...)
	{
		return {};
	}
	
}
