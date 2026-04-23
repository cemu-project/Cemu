package info.cemu.cemu.nativeinterface

import androidx.annotation.Keep

object NativeInput {
    object VPADButton {
        const val A: Int = 1
        const val B: Int = 2
        const val X: Int = 3
        const val Y: Int = 4
        const val L: Int = 5
        const val R: Int = 6
        const val ZL: Int = 7
        const val ZR: Int = 8
        const val PLUS: Int = 9
        const val MINUS: Int = 10
        const val UP: Int = 11
        const val DOWN: Int = 12
        const val LEFT: Int = 13
        const val RIGHT: Int = 14
        const val STICKL: Int = 15
        const val STICKR: Int = 16
        const val STICKL_UP: Int = 17
        const val STICKL_DOWN: Int = 18
        const val STICKL_LEFT: Int = 19
        const val STICKL_RIGHT: Int = 20
        const val STICKR_UP: Int = 21
        const val STICKR_DOWN: Int = 22
        const val STICKR_LEFT: Int = 23
        const val STICKR_RIGHT: Int = 24
        const val MIC: Int = 25
        const val SCREEN: Int = 26
        const val HOME: Int = 27
    }

    object ProButton {
        const val A: Int = 1
        const val B: Int = 2
        const val X: Int = 3
        const val Y: Int = 4
        const val L: Int = 5
        const val R: Int = 6
        const val ZL: Int = 7
        const val ZR: Int = 8
        const val PLUS: Int = 9
        const val MINUS: Int = 10
        const val HOME: Int = 11
        const val UP: Int = 12
        const val DOWN: Int = 13
        const val LEFT: Int = 14
        const val RIGHT: Int = 15
        const val STICKL: Int = 16
        const val STICKR: Int = 17
        const val STICKL_UP: Int = 18
        const val STICKL_DOWN: Int = 19
        const val STICKL_LEFT: Int = 20
        const val STICKL_RIGHT: Int = 21
        const val STICKR_UP: Int = 22
        const val STICKR_DOWN: Int = 23
        const val STICKR_LEFT: Int = 24
        const val STICKR_RIGHT: Int = 25
    }

    object ClassicButton {
        const val A: Int = 1
        const val B: Int = 2
        const val X: Int = 3
        const val Y: Int = 4
        const val L: Int = 5
        const val R: Int = 6
        const val ZL: Int = 7
        const val ZR: Int = 8
        const val PLUS: Int = 9
        const val MINUS: Int = 10
        const val HOME: Int = 11
        const val UP: Int = 12
        const val DOWN: Int = 13
        const val LEFT: Int = 14
        const val RIGHT: Int = 15
        const val STICKL_UP: Int = 16
        const val STICKL_DOWN: Int = 17
        const val STICKL_LEFT: Int = 18
        const val STICKL_RIGHT: Int = 19
        const val STICKR_UP: Int = 20
        const val STICKR_DOWN: Int = 21
        const val STICKR_LEFT: Int = 22
        const val STICKR_RIGHT: Int = 23
    }


    object WiimoteButton {
        const val A: Int = 1
        const val B: Int = 2
        const val ONE: Int = 3
        const val TWO: Int = 4
        const val NUNCHUCK_Z: Int = 5
        const val NUNCHUCK_C: Int = 6
        const val PLUS: Int = 7
        const val MINUS: Int = 8
        const val UP: Int = 9
        const val DOWN: Int = 10
        const val LEFT: Int = 11
        const val RIGHT: Int = 12
        const val NUNCHUCK_UP: Int = 13
        const val NUNCHUCK_DOWN: Int = 14
        const val NUNCHUCK_LEFT: Int = 15
        const val NUNCHUCK_RIGHT: Int = 16
        const val HOME: Int = 17
    }

    object EmulatedControllerType {
        const val VPAD: Int = 0
        const val PRO: Int = 1
        const val CLASSIC: Int = 2
        const val WIIMOTE: Int = 3
        const val DISABLED: Int = -1
    }

    object Axis {
        const val DPAD_UP: Int = 34
        const val DPAD_DOWN: Int = 35
        const val DPAD_LEFT: Int = 36
        const val DPAD_RIGHT: Int = 37
        const val X_POS: Int = 38
        const val Y_POS: Int = 39
        const val ROTATION_X_POS: Int = 40
        const val ROTATION_Y_POS: Int = 41
        const val TRIGGER_X_POS: Int = 42
        const val TRIGGER_Y_POS: Int = 43
        const val X_NEG: Int = 44
        const val Y_NEG: Int = 45
        const val ROTATION_X_NEG: Int = 46
        const val ROTATION_Y_NEG: Int = 47
    }

    const val MAX_CONTROLLERS: Int = 8
    const val MAX_VPAD_CONTROLLERS: Int = 2
    const val MAX_WPAD_CONTROLLERS: Int = 7

    @JvmStatic
    external fun onControllerKey(
        deviceDescriptor: String,
        key: Int,
        isPressed: Boolean,
    )

    @JvmStatic
    external fun onControllerAxis(
        deviceDescriptor: String,
        axis: Int,
        value: Float,
    )

    @JvmStatic
    external fun onControllerMotion(
        deviceDescriptor: String,
        timestamp: Long,
        gyroX: Float,
        gyroY: Float,
        gyroZ: Float,
        accelX: Float,
        accelY: Float,
        accelZ: Float,
    )

    @Keep
    data class ControllerInfo(
        val id: Int,
        val descriptor: String,
        val name: String,
        val hasRumble: Boolean,
        val hasMotion: Boolean,
    )

    @JvmStatic
    external fun setControllers(controllers: Array<ControllerInfo>)

    @JvmStatic
    external fun setControllerType(index: Int, emulatedControllerType: Int)

    @JvmStatic
    external fun isControllerDisabled(index: Int): Boolean

    @JvmStatic
    external fun getControllerType(index: Int): Int

    @Keep
    data class ControllerCount(
        val vpadCount: Int,
        val wpadCount: Int,
    )

    @JvmStatic
    external fun getControllersCount(): ControllerCount

    @JvmStatic
    external fun setVPADScreenToggle(index: Int, enabled: Boolean)

    @JvmStatic
    external fun getVPADScreenToggle(index: Int): Boolean

    @JvmStatic
    external fun setControllerMapping(
        deviceDescriptor: String?,
        deviceName: String?,
        index: Int,
        mappingId: Int,
        buttonId: Int,
    )

    @JvmStatic
    external fun clearControllerMapping(index: Int, mappingId: Int)

    @JvmStatic
    external fun getControllerMapping(index: Int, mappingId: Int): String

    @JvmStatic
    external fun getControllerMappings(index: Int): Map<Int, String>

    @JvmStatic
    external fun onTouchDown(x: Int, y: Int, isTV: Boolean)

    @JvmStatic
    external fun onTouchMove(x: Int, y: Int, isTV: Boolean)

    @JvmStatic
    external fun onTouchUp(x: Int, y: Int, isTV: Boolean)

    @JvmStatic
    external fun onDeviceMotion(
        timestamp: Long,
        gyroX: Float,
        gyroY: Float,
        gyroZ: Float,
        accelX: Float,
        accelY: Float,
        accelZ: Float,
    )

    @JvmStatic
    external fun setDeviceMotionEnabled(motionEnabled: Boolean)

    @JvmStatic
    external fun setDeviceRumble(rumble: Float)

    @JvmStatic
    external fun getDeviceRumble(): Float

    @JvmStatic
    external fun setDeviceControllerIndex(index: Int)

    @JvmStatic
    external fun getDeviceControllerIndex(): Int

    @JvmStatic
    external fun onOverlayButton(controllerIndex: Int, mappingId: Int, value: Boolean)

    @JvmStatic
    external fun onOverlayAxis(controllerIndex: Int, mappingId: Int, value: Float)

    @Keep
    interface ControllerCallbacks {
        fun vibrateController(descriptor: String, milliseconds: Long, amplitude: Int)
        fun cancelControllerVibration(descriptor: String)
    }

    @JvmStatic
    external fun setControllerCallbacks(callbacks: ControllerCallbacks?)

    @Keep
    interface DeviceCallbacks {
        fun vibrate(milliseconds: Long, amplitude: Int)
        fun cancelVibration()
    }

    @JvmStatic
    external fun setDeviceCallbacks(callbacks: DeviceCallbacks?)

    @Keep
    data class AxisSetting(
        val deadzone: Float = 0.25f,
        val range: Float = 1f,
    )

    @Keep
    data class ControllerSettings(
        val axis: AxisSetting = AxisSetting(),
        val rotation: AxisSetting = AxisSetting(),
        val trigger: AxisSetting = AxisSetting(),
        val rumble: Float = 0f,
        val motion: Boolean = false,
    )

    @JvmStatic
    external fun getControllerSettings(
        index: Int, descriptor: String, name: String
    ): ControllerSettings?

    @JvmStatic
    external fun setControllerSettings(
        index: Int, descriptor: String, name: String, controllerSettings: ControllerSettings
    )

    @JvmStatic
    external fun getMotionEnabledControllerDescriptors(): Array<String>

    @JvmStatic
    external fun saveInputs()
}
