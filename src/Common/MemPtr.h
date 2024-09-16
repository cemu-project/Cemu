#pragma once
#include <type_traits>
#include "betype.h"

using MPTR = uint32; // generic address in PowerPC memory space

#define MPTR_NULL (0)

using VAddr = uint32; // virtual address
using PAddr = uint32; // physical address

extern uint8* memory_base;
extern uint8* PPCInterpreterGetStackPointer();
extern uint8* PPCInterpreter_PushAndReturnStackPointer(sint32 offset);
extern void PPCInterpreterModifyStackPointer(sint32 offset);

class MEMPTRBase
{
};

template<typename T>
class MEMPTR : MEMPTRBase
{
  public:
	constexpr MEMPTR() noexcept
		: m_value(0) {}

	explicit constexpr MEMPTR(uint32 offset) noexcept
		: m_value(offset) {}

	explicit constexpr MEMPTR(const uint32be& offset) noexcept
		: m_value(offset) {}

	constexpr MEMPTR(std::nullptr_t) noexcept
		: m_value(0) {}

	MEMPTR(T* ptr) noexcept
	{
		if (ptr == nullptr)
			m_value = 0;
		else
		{
			cemu_assert_debug((uint8*)ptr >= memory_base && (uint8*)ptr <= memory_base + 0x100000000);
			m_value = (uint32)((uintptr_t)ptr - (uintptr_t)memory_base);
		}
	}

	constexpr MEMPTR(const MEMPTR&) noexcept = default;

	constexpr MEMPTR& operator=(const MEMPTR&) noexcept = default;

	constexpr MEMPTR& operator=(const uint32& offset) noexcept
	{
		m_value = offset;
		return *this;
	}

	constexpr MEMPTR& operator=(std::nullptr_t) noexcept
	{
		m_value = 0;
		return *this;
	}

	MEMPTR& operator=(T* ptr) noexcept
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

	bool atomic_compare_exchange(T* comparePtr, T* newPtr) noexcept
	{
		MEMPTR<T> mp_compare = comparePtr;
		MEMPTR<T> mp_new = newPtr;
		auto* thisValueAtomic = reinterpret_cast<std::atomic<uint32be>*>(&m_value);
		return thisValueAtomic->compare_exchange_strong(mp_compare.m_value, mp_new.m_value);
	}

	explicit constexpr operator bool() const noexcept
	{
		return m_value != 0;
	}

	// allow implicit cast to wrapped pointer type
	constexpr operator T*() const noexcept
	{
		return GetPtr();
	}

	template<typename X>
	explicit operator MEMPTR<X>() const noexcept
	{
		return MEMPTR<X>(this->m_value);
	}

	MEMPTR operator+(const MEMPTR& ptr) noexcept
	{
		return MEMPTR(this->GetMPTR() + ptr.GetMPTR());
	}
	MEMPTR operator-(const MEMPTR& ptr) noexcept
	{
		return MEMPTR(this->GetMPTR() - ptr.GetMPTR());
	}

	MEMPTR operator+(sint32 v) noexcept
	{
		// pointer arithmetic
		return MEMPTR(this->GetMPTR() + v * 4);
	}

	MEMPTR operator-(sint32 v) noexcept
	{
		// pointer arithmetic
		return MEMPTR(this->GetMPTR() - v * 4);
	}

	MEMPTR& operator+=(sint32 v) noexcept
	{
		m_value += v * sizeof(T);
		return *this;
	}

	template<typename Q = T>
	std::enable_if_t<!std::is_same_v<Q, void>, Q>& operator*() const noexcept
	{
		return *GetPtr();
	}

	constexpr T* operator->() const noexcept
	{
		return GetPtr();
	}

	template<typename Q = T>
	std::enable_if_t<!std::is_same_v<Q, void>, Q>& operator[](int index) noexcept
	{
		return GetPtr()[index];
	}

	T* GetPtr() const noexcept
	{
		return (T*)(m_value == 0 ? nullptr : memory_base + (uint32)m_value);
	}

	template<typename C>
	C* GetPtr() const noexcept
	{
		return static_cast<C*>(GetPtr());
	}

	[[nodiscard]] constexpr uint32 GetMPTR() const noexcept
	{
		return m_value.value();
	}
	[[nodiscard]] constexpr const uint32be& GetBEValue() const noexcept
	{
		return m_value;
	}

	[[nodiscard]] constexpr bool IsNull() const noexcept
	{
		return m_value == 0;
	}

  private:
	uint32be m_value;
};

static_assert(sizeof(MEMPTR<void*>) == sizeof(uint32be));
static_assert(std::is_trivially_copyable_v<MEMPTR<void*>>);

#include "StackAllocator.h"
#include "SysAllocator.h"

template<typename T>
struct fmt::formatter<MEMPTR<T>> : formatter<string_view>
{
	template<typename FormatContext>
	auto format(const MEMPTR<T>& v, FormatContext& ctx) const -> format_context::iterator
	{
		return fmt::format_to(ctx.out(), "{:#x}", v.GetMPTR());
	}
};
