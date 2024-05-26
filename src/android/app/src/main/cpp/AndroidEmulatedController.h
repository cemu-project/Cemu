#pragma once

#include "input/InputManager.h"
#include "input/api/Controller.h"
#include "input/emulated/ClassicController.h"
#include "input/emulated/ProController.h"
#include "input/emulated/WiimoteController.h"

class AndroidEmulatedController {
  private:
	size_t m_index;
	static std::array<std::unique_ptr<AndroidEmulatedController>, InputManager::kMaxController> s_emulatedControllers;
	EmulatedControllerPtr m_emulatedController;
	AndroidEmulatedController(size_t index)
		: m_index(index)
	{
		m_emulatedController = InputManager::instance().get_controller(m_index);
	}

  public:
	static AndroidEmulatedController& getAndroidEmulatedController(size_t index)
	{
		auto& controller = s_emulatedControllers.at(index);
		if (!controller)
			controller = std::unique_ptr<AndroidEmulatedController>(new AndroidEmulatedController(index));
		return *controller;
	}
	void setButtonValue(uint64 mappingId, bool value)
	{
		if (!m_emulatedController)
			return;
		m_emulatedController->setButtonValue(mappingId, value);
	}
	void setAxisValue(uint64 mappingId, float value)
	{
		if (!m_emulatedController)
			return;
		m_emulatedController->setAxisValue(mappingId, value);
	}
	void setType(EmulatedController::Type type)
	{
		if (m_emulatedController && m_emulatedController->type() == type)
			return;
		m_emulatedController = InputManager::instance().set_controller(m_index, type);
		InputManager::instance().save(m_index);
	}
	void setMapping(uint64 mappingId, ControllerPtr controller, uint64 buttonId)
	{
		if (m_emulatedController && controller)
		{
			const auto& controllers = m_emulatedController->get_controllers();
			auto controllerIt = std::find_if(controllers.begin(), controllers.end(), [&](const ControllerPtr& c) { return c->api() == controller->api() && c->uuid() == controller->uuid(); });
			if (controllerIt == controllers.end())
				m_emulatedController->add_controller(controller);
			else
				controller = *controllerIt;
			m_emulatedController->set_mapping(mappingId, controller, buttonId);
			InputManager::instance().save(m_index);
		}
	}
	std::optional<std::string> getMapping(uint64 mapping) const
	{
		if (!m_emulatedController)
			return {};
		auto controller = m_emulatedController->get_mapping_controller(mapping);
		if (!controller)
			return {};
		auto mappingName = m_emulatedController->get_mapping_name(mapping);
		return fmt::format("{}: {}", controller->display_name(), mappingName);
	}
	std::map<uint64, std::string> getMappings() const
	{
		if (!m_emulatedController)
			return {};
		std::map<uint64, std::string> mappings;
		auto type = m_emulatedController->type();
		uint64 mapping = 0;
		uint64 maxMapping = 0;
		if (type == EmulatedController::Type::VPAD)
		{
			mapping = VPADController::ButtonId::kButtonId_A;
			maxMapping = VPADController::ButtonId::kButtonId_Max;
		}
		if (type == EmulatedController::Type::Pro)
		{
			mapping = ProController::ButtonId::kButtonId_A;
			maxMapping = ProController::ButtonId::kButtonId_Max;
		}
		if (type == EmulatedController::Type::Classic)
		{
			mapping = ClassicController::ButtonId::kButtonId_A;
			maxMapping = ClassicController::ButtonId::kButtonId_Max;
		}
		if (type == EmulatedController::Type::Wiimote)
		{
			mapping = WiimoteController::ButtonId::kButtonId_A;
			maxMapping = WiimoteController::ButtonId::kButtonId_Max;
		}
		for (; mapping < maxMapping; mapping++)
		{
			auto mappingName = getMapping(mapping);
			if (mappingName.has_value())
				mappings[mapping] = mappingName.value();
		}
		return mappings;
	}
	void setDisabled()
	{
		InputManager::instance().delete_controller(m_index, true);
		m_emulatedController.reset();
	}
	EmulatedControllerPtr getEmulatedController()
	{
		return m_emulatedController;
	}

	void clearMapping(uint64 mapping)
	{
		if (!m_emulatedController)
			return;
		m_emulatedController->delete_mapping(mapping);
	}
};
