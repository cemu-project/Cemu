#pragma once

#include "input/api/ControllerProvider.h"
#include "input/api/ControllerState.h"

class AndroidControllerProvider : public ControllerProviderBase
{
    friend class AndroidController;

   public:
    inline static InputAPI::Type kAPIType = InputAPI::Android;
    InputAPI::Type api() const override { return kAPIType; }
    std::vector<std::shared_ptr<ControllerBase>> get_controllers() override { return {}; }
    void on_key_event(const std::string& deviceDescriptor, const std::string& deviceName, int nativeKeyCode, bool isPressed);
    void on_axis_event(const std::string& deviceDescriptor, const std::string& deviceName, int nativeAxisCode, float value);

   private:
    ControllerState& get_controller_state(const std::string& deviceDescriptor);
    std::mutex m_controllersMutex;
    std::unordered_map<std::string, ControllerState> m_controllersState;
};
