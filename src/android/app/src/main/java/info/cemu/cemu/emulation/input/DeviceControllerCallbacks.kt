package info.cemu.cemu.emulation.input

import android.content.Context
import android.os.VibrationEffect
import info.cemu.cemu.common.android.context.getDeviceVibrator
import info.cemu.cemu.nativeinterface.NativeInput

class DeviceControllerCallbacks(context: Context) : NativeInput.DeviceCallbacks {
    private val vibrator = context.getDeviceVibrator()
    fun register() {
        NativeInput.setDeviceCallbacks(this)
    }

    fun unregister() {
        NativeInput.setDeviceCallbacks(null)
        cancelVibration()
    }

    override fun vibrate(milliseconds: Long, amplitude: Int) {
        val effect = VibrationEffect.createOneShot(milliseconds, amplitude.coerceIn(1, 255))
        vibrator.vibrate(effect)
    }

    override fun cancelVibration() {
        vibrator.cancel()
    }
}