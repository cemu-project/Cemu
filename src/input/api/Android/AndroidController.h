#pragma once

#include "input/api/Android/AndroidControllerProvider.h"
#include "input/api/Controller.h"

class AndroidController : public Controller<AndroidControllerProvider>
{
    friend class AndroidControllerProvider;

   public:
    AndroidController(std::string_view deviceDescriptor, std::string_view deviceName);
    std::string_view api_name() const override
    {
        static_assert(to_string(InputAPI::Android) == "Android");
        return to_string(InputAPI::Android);
    }

    InputAPI::Type api() const override { return InputAPI::Android; }

    bool is_connected() override { return true; }

    bool has_axis() const override { return true; }

    std::string get_button_name(uint64 button) const override;

   protected:
    std::string m_deviceDescriptor;
    ControllerState m_controllerState{};
    ControllerState raw_state() override;
};
