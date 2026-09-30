#pragma once

class CafeStaticVarBase;

class CafeStaticData
{
protected:
	enum class Mode : uint8
	{
		COS = 0,
		IOSU = 1
	};

	constexpr CafeStaticData(Mode mode) : m_mode(mode) {};
	CafeStaticData(const CafeStaticData&) = delete;
	CafeStaticData& operator=(const CafeStaticData&) = delete;

	~CafeStaticData();

public:
	void Allocate();
	void Deallocate();
	bool IsAllocated() const { return m_allocation != nullptr; }

	static bool HasAnyAllocations();
private:
	friend class CafeStaticVarBase;
	std::vector<CafeStaticVarBase*>* m_vars{}; // pointer to make this class constinit compatible
	void* m_allocation{};
	Mode m_mode;
};

class CafeStaticVarBase
{
public:
	CafeStaticVarBase(const CafeStaticVarBase&) = delete;
	CafeStaticVarBase& operator=(const CafeStaticVarBase&) = delete;

protected:
	CafeStaticVarBase(CafeStaticData& owner, uint32 size, uint32 alignment);
	virtual ~CafeStaticVarBase();
	MEMPTR<void> m_memory;

private:
	friend class CafeStaticData;
	virtual void Initialize() = 0;
	CafeStaticData* m_owner{};
	CafeStaticVarBase* m_next{};
	uint32 m_size;
	uint32 m_alignment;
};

template<typename T, size_t count = 1, size_t alignment = 32>
class CafeStaticVar : public CafeStaticVarBase
{
	static_assert(count > 0 && count <= std::numeric_limits<uint32>::max() / sizeof(T));
	static_assert(alignment > 0 && (alignment & (alignment - 1)) == 0);
	static_assert(alignment >= alignof(T) && alignment <= 0x400000);
	static_assert(std::is_trivially_copyable_v<T> && std::is_trivially_destructible_v<T>);
	static_assert(!std::is_const_v<T>);

public:
	explicit CafeStaticVar(CafeStaticData& owner) : CafeStaticVarBase(owner, GetByteSize(), alignment)
	{
		cemu_assert_debug(!owner.IsAllocated());
	}

	CafeStaticVar(CafeStaticData& owner, const T& value) requires(count == 1) : CafeStaticVar(owner)
	{
		cemu_assert_debug(!owner.IsAllocated());
		m_initializer = [value](T* ptr) { std::construct_at(ptr, value); };
	}

	CafeStaticVar(CafeStaticData& owner, std::initializer_list<T> values) requires(count != 1) : CafeStaticVar(owner)
	{
		cemu_assert_debug(!owner.IsAllocated());
		cemu_assert(values.size() <= count);
		std::array<T, count> initial{};
		std::copy(values.begin(), values.end(), initial.begin());
		m_initializer = [initial](T* ptr) { std::uninitialized_copy_n(initial.data(), count, ptr); };
	}

	template<size_t N>
	CafeStaticVar(CafeStaticData& owner, const char (&value)[N]) requires(std::is_same_v<T, char>) : CafeStaticVar(owner)
	{
		cemu_assert_debug(!owner.IsAllocated());
		static_assert(N <= count);
		std::array<char, count> initial{};
		std::copy_n(value, N, initial.begin());
		m_initializer = [initial](T* ptr) { std::uninitialized_copy_n(initial.data(), count, ptr); };
	}

	template<typename F>
	CafeStaticVar(CafeStaticData& owner, F initializer)	requires(count == 1 && std::is_invocable_r_v<T, F&>) : CafeStaticVar(owner)
	{
		cemu_assert_debug(!owner.IsAllocated());
		m_initializer = [initializer](T* ptr) mutable { std::construct_at(ptr, initializer()); };
	}

	static constexpr uint32 GetCount() { return static_cast<uint32>(count); }
	static constexpr uint32 GetByteSize() { return static_cast<uint32>(sizeof(T) * count); }
	bool IsAllocated() const { return !m_memory.IsNull(); }
	uint32 GetMPTR() const { return m_memory.GetMPTR(); }
	T* GetPtr() const { return static_cast<T*>(m_memory.GetPtr()); }

	CafeStaticVar& operator=(const T& value) requires(count == 1)
	{
		*CheckedPtr() = value;
		return *this;
	}

	operator T() const requires(count == 1) { return *CheckedPtr(); }
	operator T*() const { return GetPtr(); }
	T& operator*() const { return *CheckedPtr(); }
	T* operator->() const requires(count == 1) { return CheckedPtr(); }
	T* operator&() const requires(count == 1) { return GetPtr(); }
	T& operator[](size_t index) const
	{
		cemu_assert(index < count);
		return CheckedPtr()[index];
	}
	T* operator+(sint32 offset) const { return CheckedPtr() + offset; }
	T* operator-(sint32 offset) const { return CheckedPtr() - offset; }

private:
	T* CheckedPtr() const
	{
		cemu_assert_debug(IsAllocated());
		return GetPtr();
	}

	void Initialize() override
	{
		if (m_initializer)
			m_initializer(GetPtr());
		else
			std::uninitialized_value_construct_n(GetPtr(), count);
	}

	std::function<void(T*)> m_initializer;
};

template<size_t N>
CafeStaticVar(CafeStaticData&, const char (&)[N]) -> CafeStaticVar<char, N>;

// IOSU storage
class IOSUStaticData : public CafeStaticData
{
public:
	constexpr IOSUStaticData() : CafeStaticData(CafeStaticData::Mode::IOSU)
	{

	}
};

// COS storage
class COSStaticData : public CafeStaticData
{
public:
	constexpr COSStaticData() : CafeStaticData(CafeStaticData::Mode::COS)
	{

	}
};