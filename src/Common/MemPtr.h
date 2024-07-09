#pragma once
#include <type_traits>
#include "betype.h"

using MPTR = uint32; // generic address in PowerPC memory space

#define MPTR_NULL	(0)

using VAddr = uint32; // virtual address
using PAddr = uint32; // physical address

extern uint8* memory_base;
extern uint8* PPCInterpreterGetStackPointer();
extern uint8* PPCInterpreter_PushAndReturnStackPointer(sint32 offset);
extern void PPCInterpreterModifyStackPointer(sint32 offset);

class MEMPTRBase {};

template <typename T>
class MEMPTR : MEMPTRBase
{
public:
	constexpr MEMPTR()
		: m_value(0) { }

	explicit constexpr MEMPTR(uint32 offset)
		: m_value(offset) { }

	explicit constexpr MEMPTR(const uint32be& offset)
		: m_value(offset) { }

	constexpr MEMPTR(std::nullptr_t)
		: m_value(0) { }

	MEMPTR(T* ptr)
	{
		if (ptr == nullptr)
			m_value = 0;
		else
        {
            cemu_assert_debug((uint8*)ptr >= memory_base && (uint8*)ptr <= memory_base + 0x100000000);
            m_value = (uint32)((uintptr_t)ptr - (uintptr_t)memory_base);
        }
	}

	constexpr MEMPTR(const MEMPTR& memptr)
		: m_value(memptr.m_value) { }

	constexpr MEMPTR& operator=(const MEMPTR& memptr)
	{
		m_value = memptr.m_value;
		return *this;
	}

	constexpr MEMPTR& operator=(const uint32& offset)
	{
		m_value = offset;
		return *this;
	}

	constexpr MEMPTR& operator=(const std::nullptr_t rhs)
	{
		m_value = 0;
		return *this;
	}

	MEMPTR& operator=(T* ptr)
	{
        if (ptr == nullptr)
			m_value = 0;
		else
        {
            cemu_assert_debug((uint8*)ptr >= memory_base && (uint8*)ptr <= memory_base + 0x100000000);
            m_value = (uint32)((uintptr_t)ptr - (uintptr_t)memory_base);
        }
		return *this;
	}

	bool atomic_compare_exchange(T* comparePtr, T* newPtr)
	{
		MEMPTR<T> mp_compare = comparePtr;
		MEMPTR<T> mp_new = newPtr;
		std::atomic<uint32be>* thisValueAtomic = (std::atomic<uint32be>*)&m_value;
		return thisValueAtomic->compare_exchange_strong(mp_compare.m_value, mp_new.m_value);
	}

	explicit constexpr operator bool() const noexcept { return m_value != 0; }
	
	constexpr operator T*() const noexcept { return GetPtr(); } // allow implicit cast to wrapped pointer type


	template <typename X>
	explicit operator MEMPTR<X>() const { return MEMPTR<X>(this->m_value); }

	//bool operator==(const MEMPTR<T>& v) const { return m_value == v.m_value; }
	//bool operator==(const T* rhs) const { return (T*)(m_value == 0 ? nullptr : memory_base + (uint32)m_value) == rhs; } -> ambigious (implicit cast to T* allows for T* == T*)
	//bool operator==(std::nullptr_t rhs) const { return m_value == 0; }

	//bool operator!=(const MEMPTR<T>& v) const { return !(*this == v); }
	//bool operator!=(const void* rhs) const { return !(*this == rhs); }
	//bool operator!=(int rhs) const { return !(*this == rhs); }

	//bool operator==(const void* rhs) const { return (void*)(m_value == 0 ? nullptr : memory_base + (uint32)m_value) == rhs; }

	//explicit bool operator==(int rhs) const { return *this == (const void*)(size_t)rhs; }
	

	MEMPTR operator+(const MEMPTR& ptr) { return MEMPTR(this->GetMPTR() + ptr.GetMPTR()); }
	MEMPTR operator-(const MEMPTR& ptr) { return MEMPTR(this->GetMPTR() - ptr.GetMPTR()); }

	MEMPTR operator+(sint32 v)
	{
		// pointer arithmetic
		return MEMPTR(this->GetMPTR() + v * 4);
	}

	MEMPTR operator-(sint32 v)
	{
		// pointer arithmetic
		return MEMPTR(this->GetMPTR() - v * 4);
	}

	template <class Q = T>
	typename std::enable_if<!std::is_same<Q, void>::value, Q>::type&
	operator*() const { return *GetPtr(); }

	T* operator->() const { return GetPtr(); }

	template <class Q = T>
	typename std::enable_if<!std::is_same<Q, void>::value, Q>::type&
	operator[](int index) { return GetPtr()[index]; }

	T* GetPtr() const { return (T*)(m_value == 0 ? nullptr : memory_base + (uint32)m_value); }

	template <typename C>
	C* GetPtr() const { return (C*)(GetPtr()); }

	constexpr uint32 GetMPTR() const { return m_value.value(); }
	constexpr const uint32be& GetBEValue() const { return m_value; }

	constexpr bool IsNull() const { return m_value == 0; }

private:
	uint32be m_value;
};

static_assert(sizeof(MEMPTR<void*>) == sizeof(uint32be));

#include "StackAllocator.h"
#include "SysAllocator.h"

template <typename T>
struct fmt::formatter<MEMPTR<T>> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const MEMPTR<T>& v, FormatContext& ctx) const -> format_context::iterator { return fmt::format_to(ctx.out(), "{:#x}", v.GetMPTR()); }
};
