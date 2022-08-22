#pragma once

class MemStreamReader
{
public:
	MemStreamReader(const uint8* data, sint32 size) : m_data(data), m_size(size)
	{
		m_cursorPos = 0;
	}

	template<typename T> T readBE();
	template<typename T> T readLE();

	template<> uint8 readBE()
	{
		if (!reserveReadLength(sizeof(uint8)))
			return 0;
		uint8 v = m_data[m_cursorPos];
		m_cursorPos += sizeof(uint8);
		return v;
	}

	template<> uint16 readBE()
	{
		if (!reserveReadLength(sizeof(uint16)))
			return 0;
		const uint8* p = m_data + m_cursorPos;
		uint16 v;
		std::memcpy(&v, p, sizeof(v));
		v = _BE(v);
		m_cursorPos += sizeof(uint16);
		return v;
	}

	template<> uint32 readBE()
	{
		if (!reserveReadLength(sizeof(uint32)))
			return 0;
		const uint8* p = m_data + m_cursorPos;
		uint32 v;
		std::memcpy(&v, p, sizeof(v));
		v = _BE(v);
		m_cursorPos += sizeof(uint32);
		return v;
	}

	template<> uint64 readBE()
	{
		if (!reserveReadLength(sizeof(uint64)))
			return 0;
		const uint8* p = m_data + m_cursorPos;
		uint64 v;
		std::memcpy(&v, p, sizeof(v));
		v = _BE(v);
		m_cursorPos += sizeof(uint64);
		return v;
	}

	template<> std::string readBE()
	{
		std::string s;
		uint32 stringSize = readBE<uint32>();
		if (hasError())
			return s;
		if (stringSize >= (32 * 1024 * 1024))
		{
			// out of bounds read or suspiciously large string
			m_hasError = true;
			return std::string();
		}
		s.resize(stringSize);
		readData(s.data(), stringSize);
		return s;
	}

	template<> uint8 readLE()
	{
		return readBE<uint8>();
	}

	template<> uint32 readLE()
	{
		if (!reserveReadLength(sizeof(uint32)))
			return 0;
		const uint8* p = m_data + m_cursorPos;
		uint32 v;
		std::memcpy(&v, p, sizeof(v));
		v = _LE(v);
		m_cursorPos += sizeof(uint32);
		return v;
	}

	template<> uint64 readLE()
	{
		if (!reserveReadLength(sizeof(uint64)))
			return 0;
		const uint8* p = m_data + m_cursorPos;
		uint64 v;
		std::memcpy(&v, p, sizeof(v));
		v = _LE(v);
		m_cursorPos += sizeof(uint64);
		return v;
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
		m_buffer.resize(m_buffer.size() + size);
		uint8* p = m_buffer.data() + m_buffer.size() - size;
		memcpy(p, ptr, size);
	}

	template<typename T> void writeBE(const T& v);

	template<>
	void writeBE<uint64>(const uint64& v)
	{
		m_buffer.resize(m_buffer.size() + 8);
		uint8* p = m_buffer.data() + m_buffer.size() - 8;
		uint64 tmp = _BE(v);
		std::memcpy(p, &tmp, sizeof(tmp));
	}

	template<>
	void writeBE<uint32>(const uint32& v)
	{
		m_buffer.resize(m_buffer.size() + 4);
		uint8* p = m_buffer.data() + m_buffer.size() - 4;
		uint32 tmp = _BE(v);
		std::memcpy(p, &tmp, sizeof(tmp));
	}

	template<>
	void writeBE<uint16>(const uint16& v)
	{
		m_buffer.resize(m_buffer.size() + 2);
		uint8* p = m_buffer.data() + m_buffer.size() - 2;
		uint16 tmp = _BE(v);
		std::memcpy(p, &tmp, sizeof(tmp));
	}

	template<>
	void writeBE<uint8>(const uint8& v)
	{
		m_buffer.emplace_back(v);
	}

	template<>
	void writeBE<std::string>(const std::string& v)
	{
		writeBE<uint32>((uint32)v.size());
		writeData(v.data(), v.size());
	}

	template<typename T> void writeLE(const T& v);

	template<>
	void writeLE<uint64>(const uint64& v)
	{
		m_buffer.resize(m_buffer.size() + 8);
		uint8* p = m_buffer.data() + m_buffer.size() - 8;
		uint64 tmp = _LE(v);
		std::memcpy(p, &tmp, sizeof(tmp));
	}

	template<>
	void writeLE<uint32>(const uint32& v)
	{
		m_buffer.resize(m_buffer.size() + 4);
		uint8* p = m_buffer.data() + m_buffer.size() - 4;
		uint32 tmp = _LE(v);
		std::memcpy(p, &tmp, sizeof(tmp));
	}

	template<typename T>
	void writePODVector(const std::vector<T>& v)
	{
		writeBE<uint32>(v.size());
		writeData(v.data(), v.size() * sizeof(T));
	}

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