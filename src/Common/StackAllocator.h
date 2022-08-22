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
		m_modified_size = count * sizeof(T) * items + kStaticMemOffset * 2;

		auto tmp = PPCInterpreterGetStackPointer();
		m_ptr = (T*)(PPCInterpreterGetAndModifyStackPointer(m_modified_size) + kStaticMemOffset);
	}

	~StackAllocator()
	{
		PPCInterpreterModifyStackPointer(-m_modified_size);
	}

	T* operator->() const
	{
		return GetPointer();
	}

	T* GetPointer() const { return m_ptr; }
	uint32 GetMPTR() const { return MEMPTR<T>(m_ptr).GetMPTR(); }
	uint32 GetMPTRBE() const { return MEMPTR<T>(m_ptr).GetMPTRBE(); }
	
	operator T*() const { return GetPointer(); }
	explicit operator uint32() const { return GetMPTR(); }

private:
	static const uint32 kStaticMemOffset = 64;

	T* m_ptr;
	sint32 m_modified_size;
};