#pragma once

#include "Common/enumFlags.h"

#include <mutex>
#include <atomic>

template<typename TType>
class ConfigValueAtomic
{
public:
	constexpr ConfigValueAtomic()
		: m_value(TType()), m_init_value(TType()) {}

	constexpr ConfigValueAtomic(const TType& init_value)
		: m_value(init_value), m_init_value(init_value) {}

	constexpr ConfigValueAtomic(TType&& init_value)
		: m_value(init_value), m_init_value(init_value) {}

	ConfigValueAtomic(const ConfigValueAtomic& v)
		: m_value(v.GetValue()), m_init_value(v.m_init_value) {}

	ConfigValueAtomic& operator=(const ConfigValueAtomic& v)
	{
		SetValue(v.GetValue());
		return *this;
	}

	ConfigValueAtomic& operator=(const TType& v)
	{
		SetValue(v);
		return *this;
	}

	ConfigValueAtomic& operator=(TType&& v)
	{
		SetValue(v);
		return *this;
	}

	[[nodiscard]] inline TType GetValue() const { return m_value.load(); }
	void SetValue(const TType& v) { m_value = v; }

	[[nodiscard]] const TType& GetInitValue() const { return m_init_value; }
	operator TType() const { return m_value.load(); }

private:
	std::atomic<TType> m_value;
	const TType m_init_value;
};

template<typename TType>
class ConfigValueNoneAtomic
{
public:
	constexpr ConfigValueNoneAtomic()
		: m_value({}), m_init_value({}) {}

	constexpr ConfigValueNoneAtomic(const TType& init_value)
		: m_value(init_value), m_init_value(init_value) {}

	constexpr ConfigValueNoneAtomic(TType&& init_value)
		: m_value(init_value), m_init_value(init_value) {}

	ConfigValueNoneAtomic(const ConfigValueNoneAtomic& v)
		: m_value(v.GetValue()), m_init_value(v.GetInitValue()) {}

	ConfigValueNoneAtomic& operator=(const ConfigValueNoneAtomic& v)
	{
		SetValue(v.GetValue());
		return *this;
	}

	ConfigValueNoneAtomic& operator=(const TType& v)
	{
		SetValue(v);
		return *this;
	}

	ConfigValueNoneAtomic& operator=(TType&& v)
	{
		SetValue(v);
		return *this;
	}

	[[nodiscard]] TType GetValue() const
	{
		std::shared_lock lock(m_mutex);
		return m_value;
	}

	void SetValue(const TType& v)
	{
		std::lock_guard lock(m_mutex);
		m_value = v;
	}

	template <typename = typename std::enable_if<std::is_same_v<TType, std::string>>>
	void SetValue(std::string_view v)
	{
		std::lock_guard lock(m_mutex);
		m_value = v;
	}

    template <typename = typename std::enable_if<std::is_same_v<TType, std::wstring>>>
	void SetValue(std::wstring_view v)
	{
		std::lock_guard lock(m_mutex);
		m_value = v;
	}

	[[nodiscard]] const TType& GetInitValue() const { return m_init_value; }

	[[nodiscard]] operator TType() const
	{
		return GetValue();
	}

protected:
	mutable std::shared_mutex m_mutex;
	TType m_value;
	const TType m_init_value;
};

template<typename TType>
constexpr bool is_atomic_type_v = std::is_trivially_copyable_v<TType> && std::is_copy_constructible_v<TType> && std::is_move_constructible_v<TType> && std::is_copy_assignable_v<TType> && std::is_move_assignable_v<TType>;

template<typename TType>
class ConfigValue : public std::conditional<is_atomic_type_v<TType>, ConfigValueAtomic<TType>, ConfigValueNoneAtomic<TType>>::type
{
public:
	using base_type = typename std::conditional<is_atomic_type_v<TType>, ConfigValueAtomic<TType>, ConfigValueNoneAtomic<TType>>::type;
	using base_type::base_type;

	ConfigValue& operator=(const ConfigValue& v)
	{
		base_type::operator=(v.GetValue());
		return *this;
	}

	ConfigValue& operator=(const TType& v)
	{
		base_type::operator=(v);
		return *this;
	}

	ConfigValue& operator=(TType&& v)
	{
		base_type::operator=(v);
		return *this;
	}
};

template<typename TType>
class ConfigValueBounds : public ConfigValue<TType>
{
public:
	using base_type = ConfigValue<TType>;

	constexpr ConfigValueBounds(const TType& min, const TType& init_value, const TType& max)
		:base_type(init_value), m_min_value(min), m_max_value(max)
	{
		assert(m_min_value <= init_value && init_value <= m_max_value);
	}

	constexpr ConfigValueBounds(TType&& min, TType&& init_value, TType&& max)
		: base_type(std::forward<TType>(init_value)), m_min_value(min), m_max_value(max)
	{
		assert(m_min_value <= init_value && init_value <= m_max_value);
	}

	// init from enum with iterators
	template<typename TEnum = typename std::enable_if<std::is_enum<TType>::value && EnableEnumIterators<TType>::enable, TType>>
	constexpr ConfigValueBounds()
		: base_type(), m_min_value(begin(TEnum{})), m_max_value(rbegin(TEnum{}))
	{
		assert(m_min_value <= this->GetInitValue() && this->GetInitValue() <= m_max_value);
	}

    template<typename TEnum = typename std::enable_if<std::is_enum<TType>::value && EnableEnumIterators<TType>::enable, TType>>
	constexpr ConfigValueBounds(const TType& init_value)
		: base_type(std::forward<TType>(init_value)), m_min_value(begin(init_value)), m_max_value(rbegin(init_value))
	{
		assert(m_min_value <= init_value && init_value <= m_max_value);
	}

    template<typename TEnum = typename std::enable_if<std::is_enum<TType>::value && EnableEnumIterators<TType>::enable, TType>>
	constexpr ConfigValueBounds(TType&& init_value)
		: base_type(std::forward<TType>(init_value)), m_min_value(begin(init_value)), m_max_value(rbegin(init_value))
	{
		assert(m_min_value <= init_value && init_value <= m_max_value);
	}

	ConfigValueBounds& operator=(const ConfigValueBounds& v)
	{
		base_type::operator=(v.GetValue());

		const auto& value = this->GetValue();
		if (value < GetMin() || value > GetMax())
			base_type::SetValue(this->GetInitValue());

		return *this;
	};

	ConfigValueBounds& operator=(const TType& v)
	{
		base_type::operator=(v);

		const auto& value = this->GetValue();
		if (value < GetMin() || value > GetMax())
			base_type::SetValue(this->GetInitValue());

		return *this;
	};

	ConfigValueBounds& operator=(TType&& v)
	{
		base_type::operator=(v);

		const auto& value = this->GetValue();
		if (value < GetMin() || value > GetMax())
			base_type::SetValue(this->GetInitValue());

		return *this;
	};

	[[nodiscard]] const TType& GetMax() const { return m_max_value; }
	[[nodiscard]] const TType& GetMin() const { return m_min_value; }

private:
	const TType m_min_value;
	const TType m_max_value;
};

template <typename TType>
struct fmt::formatter< ConfigValue<TType> > : formatter<TType> {
	template <typename FormatContext>
	auto format(const ConfigValue<TType>& v, FormatContext& ctx) {
		return formatter<TType>::format(v.GetValue(), ctx);
	}
};

template <typename TType>
struct fmt::formatter< ConfigValueBounds<TType> > : formatter<TType> {
	template <typename FormatContext>
	auto format(const ConfigValueBounds<TType>& v, FormatContext& ctx) {
		return formatter<TType>::format(v.GetValue(), ctx);
	}
};