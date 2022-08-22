#pragma once

#include "input/api/InputAPI.h"

class ControllerBase;

struct ControllerProviderSettings
{
	virtual ~ControllerProviderSettings() = default;
	virtual bool operator==(const ControllerProviderSettings&) const = 0;
};

class ControllerProviderBase
{
public:
	ControllerProviderBase() = default;
	virtual ~ControllerProviderBase() = default;

	virtual InputAPI::Type api() const = 0;
	std::string_view api_name() const { return to_string(api()); }

	virtual std::vector<std::shared_ptr<ControllerBase>> get_controllers() = 0;

	virtual bool has_settings() const
	{
		return false;
	}

	virtual bool operator==(const ControllerProviderBase& p) const
	{
		return api() == p.api();
	}
	virtual bool operator==(const ControllerProviderSettings& p) const
	{
		return false;
	}
};

using ControllerProviderPtr = std::shared_ptr<ControllerProviderBase>;

template <typename TSettings>
class ControllerProvider : public ControllerProviderBase
{
	using base_type = ControllerProviderBase;
public:
	ControllerProvider() = default;

	ControllerProvider(const TSettings& settings)
		: m_settings(settings)
	{}

	bool has_settings() const override
	{
		return true;
	}

	const TSettings& get_settings() const
	{
		return m_settings;
	}

	bool operator==(const ControllerProviderBase& p) const override
	{
		if (!base_type::operator==(p))
			return false;

		if (!p.has_settings())
			return false;

		auto* ptr = dynamic_cast<const ControllerProvider<TSettings>*>(&p);
		if (!ptr)
			return false;

		return base_type::operator==(p) && m_settings == ptr->m_settings;
	}

	bool operator==(const ControllerProviderSettings& p) const override
	{
		auto* ptr = dynamic_cast<const TSettings*>(&p);
		if (!ptr)
			return false;

		return m_settings == *ptr;
	}

protected:
	TSettings m_settings{};
};
