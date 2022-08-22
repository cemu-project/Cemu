#pragma once

template<typename TType>
class Singleton
{
protected:
	Singleton() = default;
	Singleton(const Singleton&) = delete;
	Singleton(Singleton&&) noexcept = delete;

public:
	static TType& instance() noexcept
	{
		static TType s_instance;
		return s_instance;
	}
};
