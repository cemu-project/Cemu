package info.cemu.cemu.common.input

import android.hardware.input.InputManager

interface InputDeviceListener : InputManager.InputDeviceListener {
    fun onInputDeviceChanged()
    override fun onInputDeviceAdded(deviceId: Int) = onInputDeviceChanged()
    override fun onInputDeviceRemoved(deviceId: Int) = onInputDeviceChanged()
    override fun onInputDeviceChanged(deviceId: Int) = onInputDeviceChanged()
}