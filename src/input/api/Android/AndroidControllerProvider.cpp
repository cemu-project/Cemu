#include "input/api/Android/AndroidControllerProvider.h"

#include <android/input.h>
#include <android/keycodes.h>

#include "input/InputManager.h"
#include "input/api/Android/AndroidController.h"
#include "input/api/Controller.h"

ControllerState& AndroidControllerProvider::get_controller_state(const std::string& deviceDescriptor)
{
    if (auto it = m_controllersState.find(deviceDescriptor); it != m_controllersState.end())
        return it->second;
    m_controllersState[deviceDescriptor] = {};
    return m_controllersState[deviceDescriptor];
}

void AndroidControllerProvider::on_key_event(const std::string& deviceDescriptor, const std::string& deviceName, int nativeKeyCode, bool isPressed)
{
    auto& controllerState = get_controller_state(deviceDescriptor);
    controllerState.buttons.SetButtonState(nativeKeyCode, isPressed);
}

void AndroidControllerProvider::on_axis_event(const std::string& deviceDescriptor, const std::string& deviceName, int nativeAxisCode, float value)
{
    auto& controllerState = get_controller_state(deviceDescriptor);
    switch (nativeAxisCode)
    {
        case AMOTION_EVENT_AXIS_X:
            controllerState.axis.x = value;
            break;
        case AMOTION_EVENT_AXIS_Y:
            controllerState.axis.y = value;
            break;
        case AMOTION_EVENT_AXIS_RX:
        case AMOTION_EVENT_AXIS_Z:
            controllerState.rotation.x = value;
            break;
        case AMOTION_EVENT_AXIS_RY:
        case AMOTION_EVENT_AXIS_RZ:
            controllerState.rotation.y = value;
            break;
        case AMOTION_EVENT_AXIS_LTRIGGER:
            controllerState.trigger.x = value;
            break;
        case AMOTION_EVENT_AXIS_RTRIGGER:
            controllerState.trigger.y = value;
            break;
        case AMOTION_EVENT_AXIS_HAT_X:
            if (value == 0.0f)
            {
                controllerState.buttons.SetButtonState(kButtonRight, false);
                controllerState.buttons.SetButtonState(kButtonLeft, false);
            }
            else if (value > 0.0f)
            {
                controllerState.buttons.SetButtonState(kButtonRight, false);
            }
            else
            {
                controllerState.buttons.SetButtonState(kButtonLeft, false);
            }
            break;
        case AMOTION_EVENT_AXIS_HAT_Y:
            if (value == 0.0f)
            {
                controllerState.buttons.SetButtonState(kButtonUp, false);
                controllerState.buttons.SetButtonState(kButtonDown, false);
            }
            else if (value > 0.0f)
            {
                controllerState.buttons.SetButtonState(kButtonDown, false);
            }
            else
            {
                controllerState.buttons.SetButtonState(kButtonUp, false);
            }
            break;
    }
}
