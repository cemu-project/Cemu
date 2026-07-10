package info.cemu.cemu.common.android.inputdevice

import android.hardware.Sensor
import android.os.Build
import android.os.Vibrator
import android.view.InputDevice
import info.cemu.cemu.common.collections.mapNotNull
import info.cemu.cemu.nativeinterface.NativeInput

fun InputDevice.isGameController(): Boolean {
    if (isVirtual) {
        return false
    }

    return (sources and InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD
            || (sources and InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK
            || (sources and InputDevice.SOURCE_DPAD) == InputDevice.SOURCE_DPAD
}

fun InputDevice.toControllerInfo(): NativeInput.ControllerInfo {
    require(isGameController())

    return NativeInput.ControllerInfo(
        id = id,
        descriptor = descriptor,
        name = name,
        hasRumble = hasRumble(),
        hasMotion = hasMotion(),
    )
}

fun listGameControllers() = InputDevice.getDeviceIds()
    .mapNotNull { InputDevice.getDevice(it) }
    .filter { it.isGameController() }
    .sortedWith(compareBy({ it.name }, { it.descriptor }))

fun InputDevice.hasRumble(): Boolean {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
        return vibratorManager.vibratorIds.isNotEmpty()
    }

    return false
}

fun InputDevice.hasMotion() =
    hasSensor(Sensor.TYPE_ACCELEROMETER) && hasSensor(Sensor.TYPE_GYROSCOPE)

fun InputDevice.hasSensor(type: Int) = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
    sensorManager.getDefaultSensor(type) != null
} else {
    false
}

fun InputDevice.tryUseVibrator(block: Vibrator.() -> Unit) {
    val vibrator = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
        vibratorManager.defaultVibrator
    } else {
        @Suppress("DEPRECATION")
        vibrator
    }

    if (!vibrator.hasVibrator()) {
        return
    }

    block(vibrator)
}
