#pragma once

#include <mutex>

template<typename T, uint32 elements, typename P = uint32>
class RingBuffer
{
public:
	RingBuffer();

	bool Push(const T& v);

	template<class Q = T>
	typename std::enable_if< !std::is_array<T>::value, Q >::type
	Pop()
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		if (m_readPointer == m_writePointer)
		{
			return T();
		}

		const T& tmp = m_data[m_readPointer];
		m_readPointer = (m_readPointer + 1) % elements;
		return tmp;
	}

	T& GetSlot();
	T& GetSlotAndAdvance();
	void Advance();

	void Clear();
	P GetReadPointer();
	P GetWritePointer();
	bool HasData();

private:
	T m_data[elements];
	P m_readPointer;
	P m_writePointer;
	std::mutex m_mutex;
};

template <typename T, uint32 elements, typename P>
RingBuffer<T, elements, P>::RingBuffer()
	: m_readPointer(0), m_writePointer(0)
{
	
}

template <typename T, uint32 elements, typename P>
bool RingBuffer<T, elements, P>::Push(const T& v)
{
	std::unique_lock<std::mutex> lock(m_mutex);
	if (m_readPointer == ((m_writePointer + 1) % elements))
	{
		debugBreakpoint(); // buffer is full
		return false;
	}

	m_data[m_writePointer] = v;
	m_writePointer = (m_writePointer + 1) % elements;
	return true;
}

template <typename T, uint32 elements, typename P>
T& RingBuffer<T, elements, P>::GetSlot()
{
	std::unique_lock<std::mutex> lock(m_mutex);
	T& result = m_data[m_writePointer];
	m_writePointer = (m_writePointer + 1) % elements;
	return result;
}

template <typename T, uint32 elements, typename P>
T& RingBuffer<T, elements, P>::GetSlotAndAdvance()
{
	std::unique_lock<std::mutex> lock(m_mutex);
	T& result = m_data[m_writePointer];
	m_writePointer = (m_writePointer + 1) % elements;
	m_readPointer = (m_readPointer + 1) % elements;
	return result;
}

template <typename T, uint32 elements, typename P>
void RingBuffer<T, elements, P>::Advance()
{
	std::unique_lock<std::mutex> lock(m_mutex);
	if (m_readPointer != m_writePointer)
	{
		m_readPointer = (m_readPointer + 1) % elements;
	}
}

template <typename T, uint32 elements, typename P>
void RingBuffer<T, elements, P>::Clear()
{
	std::unique_lock<std::mutex> lock(m_mutex);
	m_readPointer = 0;
	m_writePointer = 0;
}

template <typename T, uint32 elements, typename P>
P RingBuffer<T, elements, P>::GetReadPointer()
{
	std::unique_lock<std::mutex> lock(m_mutex);
	P tmp = m_readPointer;
	return tmp;
}

template <typename T, uint32 elements, typename P>
P RingBuffer<T, elements, P>::GetWritePointer()
{
	std::unique_lock<std::mutex> lock(m_mutex);
	P tmp = m_writePointer;
	return tmp;
}

template <typename T, uint32 elements, typename P>
bool RingBuffer<T, elements, P>::HasData()
{
	std::unique_lock<std::mutex> lock(m_mutex);
	return m_readPointer != m_writePointer;
}
