#pragma once

#include "input/InputManager.h"
#include "input/api/Controller.h"

ControllerPtr CreateDefaultDeviceController();

class EmulatedControllerManager
{
  private:
	size_t m_index;
	inline static std::array<std::unique_ptr<EmulatedControllerManager>, InputManager::kMaxController> s_emulatedControllers;
	EmulatedControllerPtr m_emulatedController;

	EmulatedControllerManager(size_t index);

  public:
	static EmulatedControllerManager& GetController(size_t index);

	void SetButtonValue(uint64 mappingId, bool value);

	void SetAxisValue(uint64 mappingId, float value);

	void SetType(EmulatedController::Type type);

	void SetMapping(uint64 mappingId, ControllerPtr controller, uint64 buttonId);

	std::optional<std::string> GetMapping(uint64 mapping) const;

	std::map<uint64, std::string> GetMappings() const;

	void SetDisabled();

	EmulatedControllerPtr GetControllerPtr() const;

	void ClearMapping(uint64 mapping);
};
