package info.cemu.cemu.common.android.inputevent

import android.view.InputDevice
import android.view.InputEvent

fun InputEvent.isFromPhysicalController(): Boolean {
    if (deviceId < 0 || device == null || device.isVirtual) {
        return false
    }

    return source and InputDevice.SOURCE_GAMEPAD == InputDevice.SOURCE_GAMEPAD
            || source and InputDevice.SOURCE_JOYSTICK == InputDevice.SOURCE_JOYSTICK
            || source and InputDevice.SOURCE_DPAD == InputDevice.SOURCE_DPAD
}
