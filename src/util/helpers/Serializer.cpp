#include "Serializer.h"

// read return

template<typename T>
T MemStreamReader::read()
{
	if (!reserveReadLength(sizeof(T)))
		return 0;
	const uint8* p = m_data + m_cursorPos;
	T v;
	std::memcpy(&v, p, sizeof(v));
	m_cursorPos += sizeof(T);
	return v;
}

template uint8 MemStreamReader::read<uint8>();
template uint16 MemStreamReader::read<uint16>();
template uint32 MemStreamReader::read<uint32>();
template uint32be MemStreamReader::read<uint32be>();
template uint64 MemStreamReader::read<uint64>();
template int MemStreamReader::read<int>();

template<>
std::string MemStreamReader::read()
{
	std::string s;
	uint32 stringSize = read<uint32>();
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

// read void

template<typename T>
void MemStreamReader::read(T& v)
{
	if (reserveReadLength(sizeof(T)))
	{
		const uint8* p = m_data + m_cursorPos;
		std::memcpy(&v, p, sizeof(v));
		m_cursorPos += sizeof(T);
	}
}

template void MemStreamReader::read(uint8& v);
template void MemStreamReader::read(uint16& v);
template void MemStreamReader::read(uint32& v);
template void MemStreamReader::read(uint32be& v);
template void MemStreamReader::read(uint64& v);
template void MemStreamReader::read(int& v);

template<>
void MemStreamReader::read(std::string& v)
{
	uint32 stringSize = read<uint32>();
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

// readSection

void MemStreamReader::readSection(const char* sec)
{
	std::string sec_str = std::string(sec);
	cemu_assert_debug(read<std::string>() == sec_str);
}

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
template uint32 MemStreamReader::readLE<uint32>();
template uint64 MemStreamReader::readLE<uint64>();

// write void

template<typename T>
void MemStreamWriter::write(const T& v)
{
	m_buffer.resize(m_buffer.size() + sizeof(T));
	uint8* p = m_buffer.data() + m_buffer.size() - sizeof(T);
	std::memcpy(p, &v, sizeof(v));
}

template void MemStreamWriter::write(const int& v);
template void MemStreamWriter::write(const uint64& v);
template void MemStreamWriter::write(const uint32be& v);
template void MemStreamWriter::write(const uint32& v);
template void MemStreamWriter::write(const uint16& v);
template void MemStreamWriter::write(const uint8& v);

template<>
void MemStreamWriter::write<std::string>(const std::string& v)
{
	write<uint32>((uint32)v.size());
	writeData(v.data(), v.size());
}

// writeSection

void MemStreamWriter::writeSection(const char* sec)
{
	std::string sec_str = std::string(sec);
	write(sec_str);
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

template void MemStreamWriter::writeBE(const uint64& v);
template void MemStreamWriter::writeBE(const uint32& v);
template void MemStreamWriter::writeBE(const uint16& v);
template void MemStreamWriter::writeBE(const uint8& v);

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

template void MemStreamWriter::writeLE(const uint64& v);
template void MemStreamWriter::writeLE(const uint32& v);

