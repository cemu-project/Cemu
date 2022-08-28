#include "Serializer.h"

template<>
uint8 MemStreamReader::readBE()
{
	if (!reserveReadLength(sizeof(uint8)))
		return 0;
	uint8 v = m_data[m_cursorPos];
	m_cursorPos += sizeof(uint8);
	return v;
}

template<>
uint16 MemStreamReader::readBE()
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

template<>
uint32 MemStreamReader::readBE()
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

template<>
uint64 MemStreamReader::readBE()
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

template<>
std::string MemStreamReader::readBE()
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

template<>
uint8 MemStreamReader::readLE()
{
	return readBE<uint8>();
}

template<>
uint32 MemStreamReader::readLE()
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

template<>
uint64 MemStreamReader::readLE()
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

template<>
void MemStreamWriter::writeBE<uint64>(const uint64& v)
{
	m_buffer.resize(m_buffer.size() + 8);
	uint8* p = m_buffer.data() + m_buffer.size() - 8;
	uint64 tmp = _BE(v);
	std::memcpy(p, &tmp, sizeof(tmp));
}

template<>
void MemStreamWriter::writeBE<uint32>(const uint32& v)
{
	m_buffer.resize(m_buffer.size() + 4);
	uint8* p = m_buffer.data() + m_buffer.size() - 4;
	uint32 tmp = _BE(v);
	std::memcpy(p, &tmp, sizeof(tmp));
}


template<>
void MemStreamWriter::writeBE<uint16>(const uint16& v)
{
	m_buffer.resize(m_buffer.size() + 2);
	uint8* p = m_buffer.data() + m_buffer.size() - 2;
	uint16 tmp = _BE(v);
	std::memcpy(p, &tmp, sizeof(tmp));
}


template<>
void MemStreamWriter::writeBE<uint8>(const uint8& v)
{
	m_buffer.emplace_back(v);
}


template<>
void MemStreamWriter::writeBE<std::string>(const std::string& v)
{
	writeBE<uint32>((uint32)v.size());
	writeData(v.data(), v.size());
}

template<>
void MemStreamWriter::writeLE<uint64>(const uint64& v)
{
	m_buffer.resize(m_buffer.size() + 8);
	uint8* p = m_buffer.data() + m_buffer.size() - 8;
	uint64 tmp = _LE(v);
	std::memcpy(p, &tmp, sizeof(tmp));
}


template<>
void MemStreamWriter::writeLE<uint32>(const uint32& v)
{
	m_buffer.resize(m_buffer.size() + 4);
	uint8* p = m_buffer.data() + m_buffer.size() - 4;
	uint32 tmp = _LE(v);
	std::memcpy(p, &tmp, sizeof(tmp));
}