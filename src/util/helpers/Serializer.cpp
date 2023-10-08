#include "Serializer.h"

// readBE return

template<typename T>
T MemStreamReader::readBE()
{
	if (!reserveReadLength(sizeof(T)))
		return 0;
	const uint8* p = m_data + m_cursorPos;
	T v;
	std::memcpy(&v, p, sizeof(v));
	v = _BE(v);
	m_cursorPos += sizeof(T);
	return v;
}

template uint8 MemStreamReader::readBE<uint8>();
template uint16 MemStreamReader::readBE<uint16>();
template uint32 MemStreamReader::readBE<uint32>();
template uint64 MemStreamReader::readBE<uint64>();
template int MemStreamReader::readBE<int>();

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

// readBE void

template<typename T>
void MemStreamReader::readBE(T& v)
{
	if (reserveReadLength(sizeof(T)))
	{
		const uint8* p = m_data + m_cursorPos;
		std::memcpy(&v, p, sizeof(v));
		v = _BE(v);
		m_cursorPos += sizeof(T);
	}
}

template void MemStreamReader::readBE(uint8& v);
template void MemStreamReader::readBE(uint16& v);
template void MemStreamReader::readBE(uint32& v);
template void MemStreamReader::readBE(uint64& v);
template void MemStreamReader::readBE(int& v);

template<>
void MemStreamReader::readBE(std::string& v)
{
	uint32 stringSize = readBE<uint32>();
	if (!hasError())
	{
		if (stringSize >= (32 * 1024 * 1024))
		{
			// out of bounds read or suspiciously large string
			m_hasError = true;
		}
		else
		{
			v.resize(stringSize);
			readData(v.data(), stringSize);
		}
	}
}

// readLE return

template<typename T>
T MemStreamReader::readLE()
{
	if (!reserveReadLength(sizeof(T)))
		return 0;
	const uint8* p = m_data + m_cursorPos;
	T v;
	std::memcpy(&v, p, sizeof(v));
	v = _LE(v);
	m_cursorPos += sizeof(T);
	return v;
}

template uint8 MemStreamReader::readLE<uint8>();
template uint16 MemStreamReader::readLE<uint16>();
template uint32 MemStreamReader::readLE<uint32>();

// readSection

void MemStreamReader::readSection(const char* sec)
{
	std::string sec_str = std::string(sec);
	cemu_assert_debug(readBE<std::string>() == sec_str);
}

// writeBE void

template<typename T>
void MemStreamWriter::writeBE(const T& v)
{
	m_buffer.resize(m_buffer.size() + sizeof(T));
	uint8* p = m_buffer.data() + m_buffer.size() - sizeof(T);
	T tmp = _BE(v);
	std::memcpy(p, &tmp, sizeof(tmp));
}

template void MemStreamWriter::writeBE(const uint8& v);
template void MemStreamWriter::writeBE(const uint16& v);
template void MemStreamWriter::writeBE(const uint32& v);
template void MemStreamWriter::writeBE(const uint64& v);
template void MemStreamWriter::writeBE(const int& v);

template<>
void MemStreamWriter::writeBE<std::string>(const std::string& v)
{
	writeBE<uint32>((uint32)v.size());
	writeData(v.data(), v.size());
}

// writeLE void

template<typename T>
void MemStreamWriter::writeLE(const T& v)
{
	m_buffer.resize(m_buffer.size() + sizeof(T));
	uint8* p = m_buffer.data() + m_buffer.size() - sizeof(T);
	T tmp = _LE(v);
	std::memcpy(p, &tmp, sizeof(tmp));
}

template void MemStreamWriter::writeLE(const uint8& v);
template void MemStreamWriter::writeLE(const uint16& v);
template void MemStreamWriter::writeLE(const uint32& v);

// writeSection

void MemStreamWriter::writeSection(const char* sec)
{
	std::string sec_str = std::string(sec);
	writeBE(sec_str);
}