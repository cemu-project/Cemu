#pragma once

template<typename T, int count = 1>
class StackAllocator
{
public:
	StackAllocator()
		: StackAllocator(1)
	{}

	explicit StackAllocator(const uint32 items)
	{
		m_items = items;
		m_modified_size = count * sizeof(T) * items + kStaticMemOffset * 2;
		m_modified_size = (m_modified_size/8+7) * 8; // pad to 8 bytes
		m_ptr = new(PPCInterpreter_PushAndReturnStackPointer(m_modified_size) + kStaticMemOffset) T[count * items]();
	}

	~StackAllocator()
	{
		for (size_t i = 0; i < count * m_items; ++i)
			m_ptr[i].~T();
		PPCInterpreterModifyStackPointer(-m_modified_size);
	}

	T* operator->() const
	{
		return GetPointer();
	}

	T* GetPointer() const { return m_ptr; }
	uint32 GetMPTR() const { return MEMPTR<T>(m_ptr).GetMPTR(); }

	T* operator&() { return GetPointer(); }
	explicit operator T*() const { return GetPointer(); }
	explicit operator uint32() const { return GetMPTR(); }
	explicit operator bool() const { return *m_ptr != 0; }

	// for arrays (count > 1) allow direct access via [] operator
	template<int c = count>
	requires (c > 1)
	T& operator[](const uint32 index)
	{
		return m_ptr[index];
	}

	// if count is 1, then allow direct value assignment via = operator
	template<int c = count>
	requires (c == 1)
	T& operator=(const T& rhs)
	{
		*m_ptr = rhs;
		return *m_ptr;
	}

	// if count is 1, then allow == and != operators
	template<int c = count>
	requires (c == 1)
	bool operator==(const T& rhs) const
	{
		return *m_ptr == rhs;
	}

private:
	static const uint32 kStaticMemOffset = 64;

	T* m_ptr;
	sint32 m_modified_size;
	uint32 m_items;
};