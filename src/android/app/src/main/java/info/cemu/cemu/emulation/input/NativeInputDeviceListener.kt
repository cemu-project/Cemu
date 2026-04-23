package info.cemu.cemu.emulation.input

import android.content.Context
import android.hardware.input.InputManager
import info.cemu.cemu.common.android.inputdevice.listGameControllers
import info.cemu.cemu.common.android.inputdevice.toControllerInfo
import info.cemu.cemu.common.input.InputDeviceListener
import info.cemu.cemu.nativeinterface.NativeInput

class NativeInputDeviceListener(private val context: Context) {
    private val inputManager
        get() = context.getSystemService(Context.INPUT_SERVICE) as InputManager?

    private fun refreshControllers() {
        val gameControllerInfos = listGameControllers()
            .map { it.toControllerInfo() }
            .toTypedArray()

        NativeInput.setControllers(gameControllerInfos)
    }

    private val listener = object : InputDeviceListener {
        override fun onInputDeviceChanged() = refreshControllers()
    }

    fun register() {
        inputManager?.registerInputDeviceListener(listener, null)
        refreshControllers()
    }

    fun unregister() {
        inputManager?.unregisterInputDeviceListener(listener)
    }
}