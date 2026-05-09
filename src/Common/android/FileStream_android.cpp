#include "FileStream_saf.h"
#include "FileStream_android.h"
#include "FilesystemAndroid.h"
#include "Common/unix/FileStream_unix.h"

#include <cstdarg>

class IFileStream
{
  public:
	virtual ~IFileStream() = default;

	virtual void SetPosition(uint64 pos) = 0;
	virtual uint64 GetSize() = 0;
	virtual bool SetEndOfFile() = 0;
	virtual void extract(std::vector<uint8>& data) = 0;
	virtual void Flush() = 0;

	virtual uint32 readData(void* data, uint32 length) = 0;
	virtual bool readU64(uint64& v) = 0;
	virtual bool readU32(uint32& v) = 0;
	virtual bool readU8(uint8& v) = 0;
	virtual bool readLine(std::string& line) = 0;

	virtual sint32 writeData(const void* data, sint32 length) = 0;
	virtual void writeU64(uint64 v) = 0;
	virtual void writeU32(uint32 v) = 0;
	virtual void writeU8(uint8 v) = 0;
	virtual void writeStringFmt(const char* format, ...) = 0;
	virtual void writeString(const char* str) = 0;
	virtual void writeLine(const char* str) = 0;
};

template<typename Impl>
class FileStreamAdapter : public IFileStream
{
  public:
	explicit FileStreamAdapter(Impl* impl) : m_impl(impl) {}
	~FileStreamAdapter() override
	{
		delete m_impl;
	}

	void SetPosition(uint64 pos) override
	{
		m_impl->SetPosition(pos);
	}
	uint64 GetSize() override
	{
		return m_impl->GetSize();
	}
	bool SetEndOfFile() override
	{
		return m_impl->SetEndOfFile();
	}
	void extract(std::vector<uint8>& data) override
	{
		m_impl->extract(data);
	}
	void Flush() override
	{
		m_impl->Flush();
	}

	uint32 readData(void* data, uint32 length) override
	{
		return m_impl->readData(data, length);
	}
	bool readU64(uint64& v) override
	{
		return m_impl->readU64(v);
	}
	bool readU32(uint32& v) override
	{
		return m_impl->readU32(v);
	}
	bool readU8(uint8& v) override
	{
		return m_impl->readU8(v);
	}
	bool readLine(std::string& line) override
	{
		return m_impl->readLine(line);
	}

	sint32 writeData(const void* data, sint32 length) override
	{
		return m_impl->writeData(data, length);
	}
	void writeU64(uint64 v) override
	{
		m_impl->writeU64(v);
	}
	void writeU32(uint32 v) override
	{
		m_impl->writeU32(v);
	}
	void writeU8(uint8 v) override
	{
		m_impl->writeU8(v);
	}
	void writeStringFmt(const char* format, ...) override
	{
		va_list arg_list;
		va_start(arg_list, format);
		m_impl->writeStringFmt(format, arg_list);
		va_end(arg_list);
	}
	void writeString(const char* str) override
	{
		m_impl->writeString(str);
	}
	void writeLine(const char* str) override
	{
		m_impl->writeLine(str);
	}

  private:
	Impl* m_impl;
};

FileStreamAndroid::FileStreamAndroid(IFileStream* impl) : m_impl(impl) {}

FileStreamAndroid::~FileStreamAndroid()
{
	if (m_impl)
		delete m_impl;
}

FileStreamAndroid* FileStreamAndroid::openFile(std::string_view path)
{
	return openFile2(path, false);
}

FileStreamAndroid* FileStreamAndroid::openFile(const wchar_t* path, bool allowWrite)
{
	return openFile2(path, allowWrite);
}

FileStreamAndroid* FileStreamAndroid::openFile2(const fs::path& path, bool allowWrite)
{
	if (FilesystemAndroid::IsContentUri(path))
	{
		if (allowWrite || FilesystemAndroid::IsDirectory(path))
		{
			return nullptr;
		}

		auto fileStream = FileStreamSAF::OpenFile(path);
		if (fileStream == nullptr)
		{
			return nullptr;
		}

		return new FileStreamAndroid(new FileStreamAdapter(fileStream));
	}

	auto fileStream = FileStreamUnix::openFile2(path, allowWrite);
	if (fileStream == nullptr)
	{
		return nullptr;
	}

	return new FileStreamAndroid(new FileStreamAdapter(fileStream));
}

FileStreamAndroid* FileStreamAndroid::createFile(const wchar_t* path)
{
	return createFile2(path);
}

FileStreamAndroid* FileStreamAndroid::createFile(std::string_view path)
{
	return createFile2(path);
}

FileStreamAndroid* FileStreamAndroid::createFile2(const fs::path& path)
{
	if (FilesystemAndroid::IsContentUri(path))
	{
		throw std::runtime_error("write operation not supported");
	}

	auto fileStream = FileStreamUnix::createFile2(path);
	if (fileStream == nullptr)
	{
		return nullptr;
	}

	IFileStream* adapter = new FileStreamAdapter(FileStreamUnix::createFile2(path));

	return new FileStreamAndroid(adapter);
}

std::optional<std::vector<uint8>> FileStreamAndroid::LoadIntoMemory(const fs::path& path)
{
	auto fs = openFile2(path);
	if (!fs)
		return std::nullopt;
	uint64 fileSize = fs->GetSize();
	if (fileSize > 0xFFFFFFFFull)
	{
		delete fs;
		return std::nullopt;
	}
	std::optional<std::vector<uint8>> v(fileSize);
	if (fs->readData(v->data(), (uint32)fileSize) != (uint32)fileSize)
	{
		delete fs;
		return std::nullopt;
	}
	delete fs;
	return v;
}

// size and seek
void FileStreamAndroid::SetPosition(uint64 pos)
{
	m_impl->SetPosition(pos);
}

uint64 FileStreamAndroid::GetSize()
{
	return m_impl->GetSize();
}
bool FileStreamAndroid::SetEndOfFile()
{
	return m_impl->SetEndOfFile();
}
void FileStreamAndroid::extract(std::vector<uint8>& data)
{
	return m_impl->extract(data);
}

void FileStreamAndroid::Flush()
{
	return m_impl->Flush();
}

// reading
uint32 FileStreamAndroid::readData(void* data, uint32 length)
{
	return m_impl->readData(data, length);
}
bool FileStreamAndroid::readU64(uint64& v)
{
	return m_impl->readU64(v);
}
bool FileStreamAndroid::readU32(uint32& v)
{
	return m_impl->readU32(v);
}
bool FileStreamAndroid::readU8(uint8& v)
{
	return m_impl->readU8(v);
}
bool FileStreamAndroid::readLine(std::string& line)
{
	return m_impl->readLine(line);
}

// writing (binary)
sint32 FileStreamAndroid::writeData(const void* data, sint32 length)
{
	return m_impl->writeData(data, length);
}
void FileStreamAndroid::writeU64(uint64 v)
{
	m_impl->writeU64(v);
}
void FileStreamAndroid::writeU32(uint32 v)
{
	m_impl->writeU32(v);
}
void FileStreamAndroid::writeU8(uint8 v)
{
	m_impl->writeU8(v);
}
// writing (strings)
void FileStreamAndroid::writeStringFmt(const char* format, ...)
{
	va_list arg_list;
	va_start(arg_list, format);
	m_impl->writeStringFmt(format, arg_list);
	va_end(arg_list);
}

void FileStreamAndroid::writeString(const char* str)
{
	m_impl->writeString(str);
}
void FileStreamAndroid::writeLine(const char* str)
{
	m_impl->writeLine(str);
}
