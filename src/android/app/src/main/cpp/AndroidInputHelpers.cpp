#include "AndroidInputHelpers.h"

#include "input/ControllerFactory.h"
#include "input/emulated/ClassicController.h"
#include "input/emulated/ProController.h"
#include "input/emulated/WiimoteController.h"

ControllerPtr CreateDefaultDeviceController()
{
	auto controller = ControllerFactory::CreateController(InputAPI::Device, "device", "device");
	controller->set_use_motion(true);
	return controller;
}

EmulatedControllerManager::EmulatedControllerManager(size_t index) : m_index(index)
{
	m_emulatedController = InputManager::instance().get_controller(m_index);
}

EmulatedControllerManager& EmulatedControllerManager::GetController(size_t index)
{
	auto& controller = s_emulatedControllers.at(index);

	if (!controller)
	{
		controller = std::unique_ptr<EmulatedControllerManager>(new EmulatedControllerManager(index));
	}

	return *controller;
}

void EmulatedControllerManager::SetButtonValue(uint64 mappingId, bool value)
{
	if (!m_emulatedController)
	{
		return;
	}

	m_emulatedController->set_button_value(mappingId, value);
}

void EmulatedControllerManager::SetAxisValue(uint64 mappingId, float value)
{
	if (!m_emulatedController)
	{
		return;
	}

	m_emulatedController->set_axis_value(mappingId, value);
}

void EmulatedControllerManager::SetType(EmulatedController::Type type)
{
	if (m_emulatedController && m_emulatedController->type() == type)
	{
		return;
	}

	m_emulatedController = InputManager::instance().set_controller(m_index, type);
}

void EmulatedControllerManager::SetMapping(uint64 mappingId, ControllerPtr controller, uint64 buttonId)
{
	if (!m_emulatedController || !controller)
	{
		return;
	}

	const auto& controllers = m_emulatedController->get_controllers();
	auto controllerIt = std::find_if(controllers.begin(), controllers.end(), [&](const ControllerPtr& c) { return c->api() == controller->api() && c->uuid() == controller->uuid(); });
	if (controllerIt == controllers.end())
		m_emulatedController->add_controller(controller);
	else
		controller = *controllerIt;
	m_emulatedController->set_mapping(mappingId, controller, buttonId);
}

std::optional<std::string> EmulatedControllerManager::GetMapping(uint64 mapping) const
{
	if (!m_emulatedController)
		return {};
	auto controller = m_emulatedController->get_mapping_controller(mapping);
	if (!controller)
		return {};
	auto mappingName = m_emulatedController->get_mapping_name(mapping);
	return fmt::format("{}: {}", controller->display_name(), mappingName);
}

std::map<uint64, std::string> EmulatedControllerManager::GetMappings() const
{
	if (!m_emulatedController)
		return {};

	std::map<uint64, std::string> mappings;
	EmulatedController::Type type = m_emulatedController->type();
	uint64 mapping = 0;
	uint64 maxMapping = 0;

	if (type == EmulatedController::Type::VPAD)
	{
		mapping = VPADController::ButtonId::kButtonId_A;
		maxMapping = VPADController::ButtonId::kButtonId_Max;
	}
	else if (type == EmulatedController::Type::Pro)
	{
		mapping = ProController::ButtonId::kButtonId_A;
		maxMapping = ProController::ButtonId::kButtonId_Max;
	}
	else if (type == EmulatedController::Type::Classic)
	{
		mapping = ClassicController::ButtonId::kButtonId_A;
		maxMapping = ClassicController::ButtonId::kButtonId_Max;
	}
	else if (type == EmulatedController::Type::Wiimote)
	{
		mapping = WiimoteController::ButtonId::kButtonId_A;
		maxMapping = WiimoteController::ButtonId::kButtonId_Max;
	}

	for (; mapping < maxMapping; mapping++)
	{
		if (auto mappingName = GetMapping(mapping))
		{
			mappings[mapping] = *mappingName;
		}
	}

	return mappings;
}

void EmulatedControllerManager::SetDisabled()
{
	InputManager::instance().delete_controller(m_index, true);
	m_emulatedController.reset();
}

EmulatedControllerPtr EmulatedControllerManager::GetControllerPtr() const
{
	return m_emulatedController;
}

void EmulatedControllerManager::ClearMapping(uint64 mapping)
{
	if (!m_emulatedController)
	{
		return;
	}

	m_emulatedController->delete_mapping(mapping);
}
