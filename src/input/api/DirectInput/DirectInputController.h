#pragma once

#include "input/api/DirectInput/DirectInputControllerProvider.h"
#include "input/api/Controller.h"

class DirectInputController : public Controller<DirectInputControllerProvider>
{
public:
	DirectInputController(const GUID& guid);
	DirectInputController(const GUID& guid, std::string_view display_name);
	~DirectInputController() override;
	
	std::string_view api_name() const override
	{
		static_assert(to_string(InputAPI::DirectInput) == "DirectInput");
		return to_string(InputAPI::DirectInput);
	}
	InputAPI::Type api() const override { return InputAPI::DirectInput; }

	void save(pugi::xml_node& node) override;
	void load(const pugi::xml_node& node) override;
	
	bool connect() override;
	bool is_connected() override;

	bool has_rumble() override;
	
	void start_rumble() override;
	void stop_rumble() override;

	std::string get_button_name(uint64 button) const override;

	const GUID& get_guid() const { return m_guid; }
	const GUID& get_product_guid() const { return m_product_guid; }

protected:
	ControllerState raw_state() override;

private:
	GUID m_guid;
	GUID m_product_guid{};

	std::shared_mutex m_mutex;
	LPDIRECTINPUTDEVICE8 m_device = nullptr;
	LPDIRECTINPUTEFFECT m_effect = nullptr;

	std::array<LONG, 6> m_min_axis{};
	std::array<LONG, 6> m_max_axis{};
};