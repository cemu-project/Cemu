#pragma once

class MemStreamReader
{
public:
	MemStreamReader(const uint8* data, sint32 size) : m_data(data), m_size(size)
	{
		m_cursorPos = 0;
	}

	template<typename T> T readBE();
	template<typename T> void readBE(T& v);
	template<typename T> T readLE();

	template<typename T>
	void readAtomic(std::atomic<T>& v)
	{
		v.store(readBE<T>());
	}

	template<typename T>
	std::vector<T> readPODVector()
	{
		uint32 numElements = readBE<uint32>();
		if (hasError())
		{
			return std::vector<T>();
		}
		std::vector<T> v;
		v.reserve(numElements);
		v.resize(numElements);
		readData(v.data(), v.size() * sizeof(T));
		return v;
	}

	template<typename T>
	void readPODVector(std::vector<T>& v)
	{
		uint32 numElements = readBE<uint32>();
		if (!hasError())
		{
			v.reserve(numElements);
			v.resize(numElements);
			readData(v.data(), v.size() * sizeof(T));
		}
	}

	template<typename T>
	void readPTR(T& v)
	{
		v = (T)(memory_base + readBE<uint32>());
	}

	template<template<typename> class C, typename T>
	void readMPTR(C<T>& v)
	{
		v = (T*)(memory_base + readBE<MPTR>());
	}

	template<template<typename, size_t, size_t> class C, typename T, size_t c, size_t a>
	void readMPTR(C<T,c,a>& v)
	{
		v = (T*)(memory_base + readBE<MPTR>());
	}

	void readBool(bool& v)
	{
		v = readBE<uint8>();
	}

	bool readBool()
	{
		return readBE<uint8>();
	}

	void readSection(const char* sec);

	// read string terminated by newline character (or end of stream)
	// will also trim off any carriage return
	std::string_view readLine()
	{
		size_t length = 0;
		if (m_cursorPos >= m_size)
		{
			m_hasError = true;
			return std::basic_string_view((const char*)nullptr, 0);
		}
		// end of line is determined by '\n'
		const char* lineStrBegin = (const char*)(m_data + m_cursorPos);
		const char* lineStrEnd = nullptr;
		while (m_cursorPos < m_size)
		{
			if (m_data[m_cursorPos] == '\n')
			{
				lineStrEnd = (const char*)(m_data + m_cursorPos);
				m_cursorPos++; // skip the newline character
				break;
			}
			m_cursorPos++;
		}
		if(lineStrEnd == nullptr)
			lineStrEnd = (const char*)(m_data + m_cursorPos);
		// truncate any '\r' at the beginning and end
		while (lineStrBegin < lineStrEnd)
		{
			if (lineStrBegin[0] != '\r')
				break;
			lineStrBegin++;
		}
		while (lineStrEnd > lineStrBegin)
		{
			if (lineStrEnd[-1] != '\r')
				break;
			lineStrEnd--;
		}
		length = (lineStrEnd - lineStrBegin);
		return std::basic_string_view((const char*)lineStrBegin, length);
	}

	bool readData(void* ptr, size_t size)
	{
		if (readBE<uint8>())
		{
			ptr = NULL;
			return false;
		}
		else
		{
			if (m_cursorPos + size > m_size)
			{
				m_cursorPos = m_size;
				m_hasError = true;
				return false;
			}
			memcpy(ptr, m_data + m_cursorPos, size);
			m_cursorPos += (sint32)size;
			return true;
		}	
	}

	std::span<uint8> readDataNoCopy(size_t size)
	{
		if (m_cursorPos + size > m_size)
		{
			m_cursorPos = m_size;
			m_hasError = true;
			return std::span<uint8>();
		}
		auto r = std::span<uint8>((uint8*)m_data + m_cursorPos, size);
		m_cursorPos += (sint32)size;
		return r;
	}

	// returns true if any of the reads was out of bounds
	bool hasError() const
	{
		return m_hasError;
	}

	bool isEndOfStream() const
	{
		return m_cursorPos == m_size;
	}

private:
	bool reserveReadLength(size_t length)
	{
		if (m_cursorPos + length > m_size)
		{
			m_cursorPos = m_size;
			m_hasError = true;
			return false;
		}
		return true;
	}

	void skipCRLF()
	{
		while (m_cursorPos < m_size)
		{
			if (m_data[m_cursorPos] != '\r' && m_data[m_cursorPos] != '\n')
				break;
			m_cursorPos++;
		}
	}

	const uint8* m_data;
	sint32 m_size;
	sint32 m_cursorPos;
	bool m_hasError{ false };
};

class MemStreamWriter
{
public:
	MemStreamWriter(size_t reservedSize)
	{
		if (reservedSize > 0)
			m_buffer.reserve(reservedSize);
		else
			m_buffer.reserve(128);
	}

	void writeData(const void* ptr, size_t size)
	{
		writeBE((uint8)(ptr == NULL));
		if (ptr)
		{
			m_buffer.resize(m_buffer.size() + size);
			uint8* p = m_buffer.data() + m_buffer.size() - size;
			memcpy(p, ptr, size);
		}
	}

	template<typename T> void writeBE(const T& v);
	template<typename T> void writeLE(const T& v);
	
	template<typename T>
	void writeAtomic(const std::atomic<T>& v)
	{
		writeBE(v.load());
	}

	template<typename T>
	void writePODVector(const std::vector<T>& v)
	{
		cemu_assert(std::is_trivial_v<T>);
		writeBE<uint32>(v.size());
		writeData(v.data(), v.size() * sizeof(T));
	}

	template<typename T>
	void writePTR(const T& v)
	{
		writeBE((uint32)((uint8*)v - (uint8*)memory_base));
	}

	template<typename T>
	void writeMPTR(const T& v)
	{
		writeBE(v.GetMPTR());
	}

	void writeBool(const bool& v)
	{
		writeBE((uint8)v);
	}

	void writeSection(const char* sec);

	// get result buffer without copy
	// resets internal state
	void getResultAndReset(std::vector<uint8>& data)
	{
		std::swap(m_buffer, data);
		m_buffer.clear();
	}

	std::span<uint8> getResult()
	{
		return std::span<uint8>(m_buffer.data(), m_buffer.size());
	}

private:
	std::vector<uint8> m_buffer;
};

class SerializerHelper 
{
public:
	bool serialize(std::vector<uint8>& data)
	{
		MemStreamWriter streamWriter(0);
		bool r = serializeImpl(streamWriter);
		if (!r)
			return false;
		streamWriter.getResultAndReset(data);
		return true;
	}

	bool deserialize(std::vector<uint8>& data)
	{
		MemStreamReader memStreamReader(data.data(), (sint32)data.size());
		return deserializeImpl(memStreamReader);
	}

protected:
	virtual bool serializeImpl(MemStreamWriter& streamWriter) = 0;
	virtual bool deserializeImpl(MemStreamReader& streamReader) = 0;
};