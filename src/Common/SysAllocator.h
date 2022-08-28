#pragma once

#include <vector>

uint32 coreinit_allocFromSysArea(uint32 size, uint32 alignment);
class SysAllocatorBase;

class SysAllocatorContainer
{
public:
	void Initialize();
	void PushSysAllocator(SysAllocatorBase* base);

	static SysAllocatorContainer& GetInstance();

private:
	std::vector<SysAllocatorBase*> m_sysAllocList;
};


class SysAllocatorBase
{
	friend class SysAllocatorContainer;
public:
	SysAllocatorBase();
	virtual ~SysAllocatorBase() = default;

private:
	virtual void Initialize() = 0;
};



template<typename T, size_t count = 1, size_t alignment = 8>
class SysAllocator : public SysAllocatorBase
{
public:
	SysAllocator()
	{
		m_tempData.resize(count);
	}

	SysAllocator(std::initializer_list<T> l)
	{
		m_tempData.reserve(count);

		m_tempData.insert(m_tempData.begin(), l);
		if (count > l.size())
			m_tempData.insert(m_tempData.end(), count - l.size(), T());
	}

	constexpr uint32 GetCount() const
	{
		return count;
	}

	constexpr size_t GetByteSize() const
	{
		return count * sizeof(T);
	}

	T* GetPtr() const
	{
		return m_sysMem.GetPtr();
	}

	uint32 GetMPTR() const
	{
		return m_sysMem.GetMPTR();
	}

	T& operator*()
	{
		return *GetPtr();
	}

	operator T*()
	{
		return GetPtr();
	}

	SysAllocator operator+(const SysAllocator& ptr)
	{
		return SysAllocator(this->GetMPTR() + ptr.GetMPTR());
	}

	SysAllocator operator-(const SysAllocator& ptr)
	{
		return SysAllocator(this->GetMPTR() - ptr.GetMPTR());
	}

	T* operator+(sint32 v)
	{
		return this->GetPtr() + v;
	}

	T* operator-(sint32 v)
	{
		return this->GetPtr() - v;
	}

	operator void*() { return m_sysMem->GetPtr(); }

	// for all arrays except bool
	template<class Q = T>
	typename std::enable_if< count != 1 && !std::is_same<Q, bool>::value, Q >::type&
		operator[](int index)
	{
		// return tmp data until we allocated in sys mem
		if (m_sysMem.GetMPTR() == 0)
		{
			return m_tempData[index];
		}

		return m_sysMem[index];
	}
private:
	SysAllocator(uint32 memptr)
		: m_sysMem(memptr)
	{}

	void Initialize() override
	{
		if (m_sysMem.GetMPTR() != 0)
			return;

		// alloc mem
		m_sysMem = { coreinit_allocFromSysArea(sizeof(T) * count, alignment) };
		// copy temp buffer to mem and clear it
		memcpy(m_sysMem.GetPtr(), m_tempData.data(), sizeof(T)*count);
		m_tempData.clear();
	}

	MEMPTR<T> m_sysMem;
	std::vector<T> m_tempData;
};

template<typename T>
class SysAllocator<T, 1> : public SysAllocatorBase
{
public:
	SysAllocator()
	{
		m_tempData = {};
	}

	SysAllocator(const T& value)
	{
		m_tempData = value;
	}

	SysAllocator& operator=(const T& value)
	{
		*m_sysMem = value;
		return *this;
	}

	operator T()
	{
		return *GetPtr();
	}

	operator T*()
	{
		return GetPtr();
	}

	T* GetPtr() const
	{
		return m_sysMem.GetPtr();
	}

	uint32 GetMPTR() const
	{
		return m_sysMem.GetMPTR();
	}

	T* operator&() { return (T*)m_sysMem.GetPtr(); }

	T* operator->() const
	{
		return this->GetPtr();
	}

private:
	void Initialize() override
	{
		if (m_sysMem.GetMPTR() != 0)
			return;

		// alloc mem
		m_sysMem = { coreinit_allocFromSysArea(sizeof(T), 8) };
		// copy temp buffer to mem and clear it
		*m_sysMem = m_tempData;
	}

	MEMPTR<T> m_sysMem;
	T m_tempData;
};