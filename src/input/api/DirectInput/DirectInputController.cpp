#include "input/api/DirectInput/DirectInputController.h"
#include "gui/guiWrapper.h"

DirectInputController::DirectInputController(const GUID& guid)
	: base_type(StringFromGUID(guid), fmt::format("[{}]", StringFromGUID(guid))),
	m_guid{ guid }
{
	
}

DirectInputController::DirectInputController(const GUID& guid, std::string_view display_name)
	: base_type(StringFromGUID(guid), display_name), m_guid(guid)
{
}

DirectInputController::~DirectInputController()
{
	if (m_effect)
		m_effect->Release();
	
	if (m_device)
	{
		m_device->Unacquire();

		// TODO: test if really needed
		// workaround for gamecube controllers crash on release?
		bool should_release_device = true;
		if (m_product_guid == GUID{}) {
			DIDEVICEINSTANCE info{};
			info.dwSize = sizeof(DIDEVICEINSTANCE);
			if (SUCCEEDED(m_device->GetDeviceInfo(&info)))
			{
				m_product_guid = info.guidProduct;
			}
		}

		// info.guidProduct = {18440079-0000-0000-0000-504944564944}
		constexpr GUID kGameCubeController = { 0x18440079, 0, 0, {0,0,0x50,0x49,0x44,0x56,0x49,0x44} };
		if (kGameCubeController == m_product_guid)
			should_release_device = false;

		if (should_release_device)
			m_device->Release();
	}
}

void DirectInputController::save(pugi::xml_node& node)
{
	base_type::save(node);

	node.append_child("product_guid").append_child(pugi::node_pcdata).set_value(
		fmt::format("{}", StringFromGUID(m_product_guid)).c_str());
}

void DirectInputController::load(const pugi::xml_node& node)
{
	base_type::load(node);

	if (const auto value = node.child("product_guid")) {
		if (GUIDFromString(value.child_value(), m_product_guid) && m_product_guid != GUID{} && !is_connected())
		{
			// test if another controller with the same product guid is connectable and replace
			for(const auto& c : m_provider->get_controllers())
			{
				if(const auto ptr = std::dynamic_pointer_cast<DirectInputController>(c))
				{
					if (ptr->is_connected() && ptr->get_product_guid() == m_product_guid)
					{
						const auto tmp_guid = m_guid;
						m_guid = ptr->get_guid();
						if (connect())
							break;

						// couldn't connect
						m_guid = tmp_guid;
					}
				}
				
			}
		}
	}
}

bool DirectInputController::connect()
{
	if (is_connected())
		return true;

	m_effect = nullptr;

	std::scoped_lock lock(m_mutex);
	HRESULT hr = m_provider->get_dinput()->CreateDevice(m_guid, &m_device, nullptr);
	if (FAILED(hr) || m_device == nullptr)
		return false;

	DIDEVICEINSTANCE idi{};
	idi.dwSize = sizeof(DIDEVICEINSTANCE);
	if (SUCCEEDED(m_device->GetDeviceInfo(&idi)))
	{
		// overwrite guid name with "real" display name
		m_display_name = boost::nowide::narrow(idi.tszProductName);
	}

	// set data format
	if (FAILED(m_device->SetDataFormat(m_provider->get_data_format())))
	{
		SAFE_RELEASE(m_device);
		return false;
	}

	HWND hwndMainWindow = gui_getWindowInfo().window_main.hwnd;

	// set access
	if (FAILED(m_device->SetCooperativeLevel(hwndMainWindow, DISCL_BACKGROUND | DISCL_EXCLUSIVE)))
	{
		if (FAILED(m_device->SetCooperativeLevel(hwndMainWindow, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE)))
		{
			SAFE_RELEASE(m_device);
			return false;
		}
		// rumble can only be used with exclusive access
	}
	else
	{
		GUID guid_effect = GUID_NULL;
		// check if constant force is supported
		HRESULT result = m_device->EnumEffects([](LPCDIEFFECTINFOW eff, LPVOID guid) -> BOOL
			{
				*(GUID*)guid = eff->guid;
				return DIENUM_STOP;
			}, &guid_effect, DIEFT_CONSTANTFORCE);

		if (SUCCEEDED(result) && guid_effect != GUID_NULL)
		{
			DWORD dwAxes[2] = { DIJOFS_X, DIJOFS_Y };
			LONG lDirection[2] = { 1, 0 };

			DICONSTANTFORCE constant_force = { DI_FFNOMINALMAX }; // DI_FFNOMINALMAX -> should be max normally?!

			DIEFFECT effect{};
			effect.dwSize = sizeof(DIEFFECT);
			effect.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
			effect.dwDuration = INFINITE; // DI_SECONDS;
			effect.dwGain = DI_FFNOMINALMAX; // No scaling
			effect.dwTriggerButton = DIEB_NOTRIGGER; // Not a button response DIEB_NOTRIGGER DIJOFS_BUTTON0
			effect.cAxes = 2;
			effect.rgdwAxes = dwAxes;
			effect.rglDirection = lDirection;
			effect.cbTypeSpecificParams = sizeof(DICONSTANTFORCE);
			effect.lpvTypeSpecificParams = &constant_force;
			m_device->CreateEffect(guid_effect, &effect, &m_effect, nullptr);
		}
	}

	DIDEVICEINSTANCE info{};
	info.dwSize = sizeof(DIDEVICEINSTANCE);
	if (SUCCEEDED(m_device->GetDeviceInfo(&info)))
	{
		m_product_guid = info.guidProduct;
	}

	std::fill(m_min_axis.begin(), m_min_axis.end(), 0);
	std::fill(m_max_axis.begin(), m_max_axis.end(), std::numeric_limits<uint16>::max());
	m_device->EnumObjects(
		[](LPCDIDEVICEOBJECTINSTANCE lpddoi, LPVOID pvRef) -> BOOL
		{
			auto* thisptr = (DirectInputController*)pvRef;

			const auto instance = DIDFT_GETINSTANCE(lpddoi->dwType);
			// some tools may use state.rglSlider properties, so they have 8 instead of 6 axis
			if(instance >= thisptr->m_min_axis.size())
			{
				return DIENUM_CONTINUE;
			}
			
			DIPROPRANGE range{};
			range.diph.dwSize = sizeof(range);
			range.diph.dwHeaderSize = sizeof(range.diph);
			range.diph.dwHow = DIPH_BYID;
			range.diph.dwObj = lpddoi->dwType;
			if (thisptr->m_device->GetProperty(DIPROP_RANGE, &range.diph) == DI_OK)
			{
				thisptr->m_min_axis[instance] = range.lMin;
				thisptr->m_max_axis[instance] = range.lMax;
			}

			return DIENUM_CONTINUE;
		}, this, DIDFT_AXIS);

	m_device->Acquire();
	return true;
}

bool DirectInputController::is_connected()
{
	std::shared_lock lock(m_mutex);
	return m_device != nullptr;
}

bool DirectInputController::has_rumble()
{
	return m_effect != nullptr;
}

void DirectInputController::start_rumble()
{
	if (!has_rumble())
		return;
}

void DirectInputController::stop_rumble()
{
	if (!has_rumble())
		return;
}

std::string DirectInputController::get_button_name(uint64 button) const
{
	switch(button)
	{
	case kAxisXP: return "X+";
	case kAxisYP: return "Y+";

	case kAxisXN: return "X-";
	case kAxisYN: return "Y-";

	case kRotationXP: return "RX+";
	case kRotationYP: return "RY+";

	case kRotationXN: return "RX-";
	case kRotationYN: return "RY-";

	case kTriggerXP: return "Z+";
	case kTriggerYP: return "RZ+";

	case kTriggerXN: return "Z-";
	case kTriggerYN: return "RZ-";
	}

	return base_type::get_button_name(button);
}

ControllerState DirectInputController::raw_state()
{
	ControllerState result{};
	if (!is_connected())
		return result;
	HRESULT hr = m_device->Poll();
	if (FAILED(hr))
	{
		if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED)
		{
			result.last_state = hr;
			m_device->Acquire();
		}

		return result;
	}

	DIJOYSTATE state{};
	hr = m_device->GetDeviceState(sizeof(state), &state);
	if (FAILED(hr))
	{
		if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED)
		{
			result.last_state = hr;
			m_device->Acquire();
		}

		return result;
	}

	result.last_state = hr;

	// buttons
	for (size_t i = 0; i < std::size(state.rgbButtons); ++i)
	{
		if (HAS_BIT(state.rgbButtons[i], 7))
			result.buttons.SetButtonState(i, true);
	}
	
	// axis
	constexpr float kThreshold = 0.001f;
	float v = (float(state.lX - m_min_axis[0]) / float(m_max_axis[0] - m_min_axis[0])) * 2.0f - 1.0f;
	if (std::abs(v) >= kThreshold)
		result.axis.x = v;

	v = (float(state.lY - m_min_axis[1]) / float(m_max_axis[1] - m_min_axis[1])) * 2.0f - 1.0f;
	if (std::abs(v) >= kThreshold)
		result.axis.y = -v;

	// Right Stick
	v = (float(state.lRx - m_min_axis[3]) / float(m_max_axis[3] - m_min_axis[3])) * 2.0f - 1.0f;
	if (std::abs(v) >= kThreshold)
		result.rotation.x = v;

	v = (float(state.lRy - m_min_axis[4]) / float(m_max_axis[4] - m_min_axis[4])) * 2.0f - 1.0f;
	if (std::abs(v) >= kThreshold)
		result.rotation.y = -v;

	// Trigger
	v = (float(state.lZ - m_min_axis[2]) / float(m_max_axis[2] - m_min_axis[2])) * 2.0f - 1.0f;
	if (std::abs(v) >= kThreshold)
		result.trigger.x = v;

	v = (float(state.lRz - m_min_axis[5]) / float(m_max_axis[5] - m_min_axis[5])) * 2.0f - 1.0f;
	if (std::abs(v) >= kThreshold)
		result.trigger.y = -v;

	// dpad
	const auto pov = state.rgdwPOV[0];
	if (pov != static_cast<DWORD>(-1))
	{
		switch (pov)
		{
		case 0: result.buttons.SetButtonState(kButtonUp, true);
			break;
		case 4500: result.buttons.SetButtonState(kButtonUp, true); // up + right
		case 9000: result.buttons.SetButtonState(kButtonRight, true);
			break;
		case 13500: result.buttons.SetButtonState(kButtonRight, true); // right + down
		case 18000: result.buttons.SetButtonState(kButtonDown, true);
			break;
		case 22500: result.buttons.SetButtonState(kButtonDown, true); // down + left
		case 27000: result.buttons.SetButtonState(kButtonLeft, true);
			break;
		case 31500: result.buttons.SetButtonState(kButtonLeft, true); // left + up
					result.buttons.SetButtonState(kButtonUp, true); // left + up
			break;
		}
	}

	return result;
}
