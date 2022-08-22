#pragma once

template<typename T>
class MemoryPool
{
	static_assert(sizeof(T) >= sizeof(void*)); // object must be large enough to store a single pointer

public:
	MemoryPool(int allocationGranularity)
		: m_numObjectsAllocated(0)
	{
		m_allocationGranularity = allocationGranularity;
		m_nextFreeObject = nullptr;
	}

	template <typename... Ts>
	T* allocObj(Ts&&... args)
	{
		if (m_nextFreeObject)
		{
			T* allocatedObj = m_nextFreeObject;
			m_nextFreeObject = *(T**)allocatedObj;
			new (allocatedObj) T(std::forward<Ts>(args)...);
			return allocatedObj;
		}
		// enlarge pool
		increasePoolSize();
		T* allocatedObj = m_nextFreeObject;
		m_nextFreeObject = *(T**)allocatedObj;
		new (allocatedObj) T(std::forward<Ts>(args)...);
		return allocatedObj;
	}

	void freeObj(T* obj)
	{
		obj->~T();
		pushElementOnFreeStack(obj);
	}

private:
	void pushElementOnFreeStack(T* obj)
	{
		*(T**)obj = m_nextFreeObject;
		m_nextFreeObject = obj;
	}

	void increasePoolSize()
	{
		T* newElements = static_cast<T*>(::operator new(m_allocationGranularity * sizeof(T), std::nothrow));
		m_numObjectsAllocated += m_allocationGranularity;
		for (int i = 0; i < m_allocationGranularity; i++)
		{
			pushElementOnFreeStack(newElements);
			newElements++;
		}
	}

private:
	T* m_nextFreeObject;
	int m_allocationGranularity;
	int m_numObjectsAllocated;
};

// this memory pool calls the default constructor when the internal pool is allocated
// no constructor/destructor will be called on acquire/release
template<typename T>
class MemoryPoolPermanentObjects
{
	struct internalObject_t
	{
		T v;
		internalObject_t* next;
	};

public:
	MemoryPoolPermanentObjects(int allocationGranularity)
		: m_numObjectsAllocated(0)
	{
		m_allocationGranularity = allocationGranularity;
		m_nextFreeObject = nullptr;
	}

	template <typename... Ts>
	T* acquireObj(Ts&&... args)
	{
		if (m_nextFreeObject)
		{
			internalObject_t* allocatedObject = m_nextFreeObject;
			m_nextFreeObject = allocatedObject->next;
			return &allocatedObject->v;
		}
		// enlarge pool
		increasePoolSize();
		internalObject_t* allocatedObject = m_nextFreeObject;
		m_nextFreeObject = allocatedObject->next;
		return &allocatedObject->v;
	}

	void releaseObj(T* obj)
	{
		internalObject_t* internalObj = (internalObject_t*)((uint8*)obj - offsetof(internalObject_t, v));
		pushElementOnFreeStack(internalObj);
	}

private:
	void pushElementOnFreeStack(internalObject_t* obj)
	{
		obj->next = m_nextFreeObject;
		m_nextFreeObject = obj;
	}

	void increasePoolSize()
	{
		internalObject_t* newElements = static_cast<internalObject_t*>(::operator new(m_allocationGranularity * sizeof(internalObject_t), std::nothrow));
		m_numObjectsAllocated += m_allocationGranularity;
		for (int i = 0; i < m_allocationGranularity; i++)
		{
			new (&newElements->v) T();
			pushElementOnFreeStack(newElements);
			newElements++;
		}
	}

private:
	internalObject_t* m_nextFreeObject;
	int m_allocationGranularity;
	int m_numObjectsAllocated;
};