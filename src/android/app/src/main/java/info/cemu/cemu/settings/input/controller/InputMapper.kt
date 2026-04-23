package info.cemu.cemu.settings.input.controller

import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import info.cemu.cemu.common.android.inputevent.isFromPhysicalController
import info.cemu.cemu.nativeinterface.NativeInput
import info.cemu.cemu.nativeinterface.NativeInput.setControllerMapping
import kotlin.math.abs

private fun getNativeAxisKey(axis: Int, isPositive: Boolean): Int? {
    return if (isPositive) {
        when (axis) {
            MotionEvent.AXIS_X -> NativeInput.Axis.X_POS
            MotionEvent.AXIS_Y -> NativeInput.Axis.Y_POS
            MotionEvent.AXIS_RX, MotionEvent.AXIS_Z -> NativeInput.Axis.ROTATION_X_POS
            MotionEvent.AXIS_RY, MotionEvent.AXIS_RZ -> NativeInput.Axis.ROTATION_Y_POS
            MotionEvent.AXIS_LTRIGGER -> NativeInput.Axis.TRIGGER_X_POS
            MotionEvent.AXIS_RTRIGGER -> NativeInput.Axis.TRIGGER_Y_POS
            MotionEvent.AXIS_HAT_X -> NativeInput.Axis.DPAD_RIGHT
            MotionEvent.AXIS_HAT_Y -> NativeInput.Axis.DPAD_DOWN
            else -> null
        }
    } else {
        when (axis) {
            MotionEvent.AXIS_X -> NativeInput.Axis.X_NEG
            MotionEvent.AXIS_Y -> NativeInput.Axis.Y_NEG
            MotionEvent.AXIS_RX, MotionEvent.AXIS_Z -> NativeInput.Axis.ROTATION_X_NEG
            MotionEvent.AXIS_RY, MotionEvent.AXIS_RZ -> NativeInput.Axis.ROTATION_Y_NEG
            MotionEvent.AXIS_HAT_X -> NativeInput.Axis.DPAD_LEFT
            MotionEvent.AXIS_HAT_Y -> NativeInput.Axis.DPAD_UP
            else -> null
        }
    }
}

private const val MIN_ABS_AXIS_VALUE = 0.33f

object InputMapper {
    fun tryMapMotionEventToMappingId(
        controllerIndex: Int,
        mappingId: Int,
        event: MotionEvent,
    ): Boolean {
        if (!event.isFromPhysicalController()) {
            return false
        }

        val device = event.device
        var maxAbsAxisValue = 0.0f
        var maxAxis = -1
        val actionPointerIndex = event.actionIndex

        for (motionRange in device.motionRanges) {
            val axisValue = event.getAxisValue(motionRange.axis, actionPointerIndex)
            val axis = getNativeAxisKey(motionRange.axis, axisValue > 0) ?: continue
            if (abs(axisValue.toDouble()) > maxAbsAxisValue) {
                maxAxis = axis
                maxAbsAxisValue = abs(axisValue.toDouble()).toFloat()
            }
        }

        if (maxAbsAxisValue > MIN_ABS_AXIS_VALUE) {
            setControllerMapping(
                device.descriptor,
                device.name,
                controllerIndex,
                mappingId,
                maxAxis
            )
            return true
        }
        return false
    }


    fun mapKeyEventToMappingId(controllerIndex: Int, mappingId: Int, event: KeyEvent) {
        if (!event.isFromPhysicalController()) {
            return
        }

        val device = event.device
        setControllerMapping(
            device.descriptor,
            device.name,
            controllerIndex,
            mappingId,
            event.keyCode
        )
    }

    private fun mapAxisCodeToMappingId(
        controllerIndex: Int,
        mappingId: Int,
        deviceDescriptor: String,
        deviceName: String,
        axisCode: Int,
        isPositive: Boolean,
    ) {
        val axis = getNativeAxisKey(axisCode, isPositive) ?: return
        setControllerMapping(
            deviceDescriptor,
            deviceName,
            controllerIndex,
            mappingId,
            axis
        )
    }

    private fun mapKeyCodeToMappingId(
        controllerIndex: Int,
        mappingId: Int,
        deviceDescriptor: String,
        deviceName: String,
        keyCode: Int,
    ) {
        setControllerMapping(
            deviceDescriptor,
            deviceName,
            controllerIndex,
            mappingId,
            keyCode
        )
    }

    fun mapAllInputs(deviceId: Int, controllerIndex: Int) {
        if (NativeInput.isControllerDisabled(controllerIndex)) {
            return
        }
        val controllerType = NativeInput.getControllerType(controllerIndex)
        val device = InputDevice.getDevice(deviceId) ?: return

        val inputs = mutableMapOf<InputMapping, Boolean>().apply {
            val buttonKeyCodes =
                ButtonInputMapping.entries.map { it.keyCode }.toIntArray()
            ButtonInputMapping.entries
                .zip(device.hasKeys(*buttonKeyCodes).toTypedArray())
                .forEach { (button, hasKey) -> put(button, hasKey) }

            device.motionRanges.forEach { motionRange ->
                AxisInputMapping.entries.filter {
                    val isPositive = motionRange.min < 0 && !it.isPositive
                    val isNegative = motionRange.max > 0 && it.isPositive
                    (isPositive || isNegative) && it.axisCode == motionRange.axis
                }.forEach { put(it, true) }
            }
        }

        val buttons = getNativeButtonsForControllerType(controllerType)

        for (button in buttons) {
            val mapping = getButtonMappings(button).firstOrNull { inputs[it] == true }
                ?: FALLBACK_BUTTONS.firstOrNull { inputs[it] == true }
            if (mapping == null) {
                continue
            }
            inputs[mapping] = false

            val buttonId = button.nativeKeyCode

            if (mapping is ButtonInputMapping) {
                mapKeyCodeToMappingId(
                    controllerIndex,
                    buttonId,
                    device.descriptor,
                    device.name,
                    mapping.keyCode
                )
            }

            if (mapping is AxisInputMapping) {
                mapAxisCodeToMappingId(
                    controllerIndex,
                    buttonId,
                    device.descriptor,
                    device.name,
                    mapping.axisCode,
                    mapping.isPositive,
                )
            }
        }
    }

    private fun getButtonMappings(button: NativeInputButton): Array<InputMapping> {
        return when (button) {
            VPadButtons.A,
            ProControllerButtons.A,
            ClassicControllerButtons.A,
            WiimoteButtons.A,
                -> arrayOf(ButtonInputMapping.BUTTON_A)

            VPadButtons.B,
            ProControllerButtons.B,
            ClassicControllerButtons.B,
            WiimoteButtons.B,
                -> arrayOf(ButtonInputMapping.BUTTON_B)

            VPadButtons.X,
            ProControllerButtons.X,
            ClassicControllerButtons.X,
            WiimoteButtons.ONE,
                -> arrayOf(ButtonInputMapping.BUTTON_X)

            VPadButtons.Y,
            ProControllerButtons.Y,
            ClassicControllerButtons.Y,
            WiimoteButtons.TWO,
                -> arrayOf(ButtonInputMapping.BUTTON_Y)

            VPadButtons.L,
            ProControllerButtons.L,
            ClassicControllerButtons.L,
            WiimoteButtons.NUNCHUCK_C,
                -> arrayOf(ButtonInputMapping.BUTTON_L1)

            VPadButtons.R,
            ProControllerButtons.R,
            ClassicControllerButtons.R,
            WiimoteButtons.NUNCHUCK_Z,
                -> arrayOf(ButtonInputMapping.BUTTON_R1)

            VPadButtons.ZL,
            ProControllerButtons.ZL,
            ClassicControllerButtons.ZL,
                -> arrayOf(ButtonInputMapping.BUTTON_L2, AxisInputMapping.LTRIGGER)

            VPadButtons.ZR,
            ProControllerButtons.ZR,
            ClassicControllerButtons.ZR,
                -> arrayOf(ButtonInputMapping.BUTTON_R2, AxisInputMapping.RTRIGGER)

            VPadButtons.PLUS,
            ProControllerButtons.PLUS,
            ClassicControllerButtons.PLUS,
            WiimoteButtons.PLUS,
                -> arrayOf(ButtonInputMapping.BUTTON_START)

            VPadButtons.MINUS,
            ProControllerButtons.MINUS,
            ClassicControllerButtons.MINUS,
            WiimoteButtons.MINUS,
                -> arrayOf(ButtonInputMapping.BUTTON_SELECT)

            VPadButtons.STICKL_UP,
            ProControllerButtons.STICKL_UP,
            ClassicControllerButtons.STICKL_UP,
            WiimoteButtons.NUNCHUCK_UP,
                -> arrayOf(AxisInputMapping.Y_NEG)

            VPadButtons.STICKL_DOWN,
            ProControllerButtons.STICKL_DOWN,
            ClassicControllerButtons.STICKL_DOWN,
            WiimoteButtons.NUNCHUCK_DOWN,
                -> arrayOf(AxisInputMapping.Y_POS)

            VPadButtons.STICKL_LEFT,
            ProControllerButtons.STICKL_LEFT,
            ClassicControllerButtons.STICKL_LEFT,
            WiimoteButtons.NUNCHUCK_LEFT,
                -> arrayOf(AxisInputMapping.X_NEG)

            VPadButtons.STICKL_RIGHT,
            ProControllerButtons.STICKL_RIGHT,
            ClassicControllerButtons.STICKL_RIGHT,
            WiimoteButtons.NUNCHUCK_RIGHT,
                -> arrayOf(AxisInputMapping.X_POS)

            VPadButtons.STICKR_UP,
            ProControllerButtons.STICKR_UP,
            ClassicControllerButtons.STICKR_UP,
                -> arrayOf(AxisInputMapping.RY_NEG, AxisInputMapping.RZ_NEG)

            VPadButtons.STICKR_DOWN,
            ProControllerButtons.STICKR_DOWN,
            ClassicControllerButtons.STICKR_DOWN,
                -> arrayOf(AxisInputMapping.RY_POS, AxisInputMapping.RZ_POS)

            VPadButtons.STICKR_LEFT,
            ProControllerButtons.STICKR_LEFT,
            ClassicControllerButtons.STICKR_LEFT,
                -> arrayOf(AxisInputMapping.RX_NEG, AxisInputMapping.Z_NEG)

            VPadButtons.STICKR_RIGHT,
            ProControllerButtons.STICKR_RIGHT,
            ClassicControllerButtons.STICKR_RIGHT,
                -> arrayOf(AxisInputMapping.RX_POS, AxisInputMapping.Z_POS)

            VPadButtons.UP,
            ProControllerButtons.UP,
            ClassicControllerButtons.UP,
            WiimoteButtons.UP,
                -> arrayOf(AxisInputMapping.HAT_Y_NEG, ButtonInputMapping.DPAD_UP)

            VPadButtons.DOWN,
            ProControllerButtons.DOWN,
            ClassicControllerButtons.DOWN,
            WiimoteButtons.DOWN,
                -> arrayOf(AxisInputMapping.HAT_Y_POS, ButtonInputMapping.DPAD_DOWN)

            VPadButtons.LEFT,
            ProControllerButtons.LEFT,
            ClassicControllerButtons.LEFT,
            WiimoteButtons.LEFT,
                -> arrayOf(AxisInputMapping.HAT_X_NEG, ButtonInputMapping.DPAD_LEFT)

            VPadButtons.RIGHT,
            ProControllerButtons.RIGHT,
            ClassicControllerButtons.RIGHT,
            WiimoteButtons.RIGHT,
                -> arrayOf(AxisInputMapping.HAT_X_POS, ButtonInputMapping.DPAD_RIGHT)

            VPadButtons.STICKL,
            ProControllerButtons.STICKL,
                -> arrayOf(ButtonInputMapping.BUTTON_THUMBL)

            VPadButtons.STICKR,
            ProControllerButtons.STICKR,
                -> arrayOf(ButtonInputMapping.BUTTON_THUMBR)

            else -> arrayOf()
        }
    }
}


private sealed interface InputMapping

private enum class ButtonInputMapping(val keyCode: Int) : InputMapping {
    BUTTON_1(KeyEvent.KEYCODE_BUTTON_1),
    BUTTON_2(KeyEvent.KEYCODE_BUTTON_2),
    BUTTON_3(KeyEvent.KEYCODE_BUTTON_3),
    BUTTON_4(KeyEvent.KEYCODE_BUTTON_4),
    BUTTON_5(KeyEvent.KEYCODE_BUTTON_5),
    BUTTON_6(KeyEvent.KEYCODE_BUTTON_6),
    BUTTON_7(KeyEvent.KEYCODE_BUTTON_7),
    BUTTON_8(KeyEvent.KEYCODE_BUTTON_8),
    BUTTON_9(KeyEvent.KEYCODE_BUTTON_9),
    BUTTON_10(KeyEvent.KEYCODE_BUTTON_10),
    BUTTON_11(KeyEvent.KEYCODE_BUTTON_11),
    BUTTON_12(KeyEvent.KEYCODE_BUTTON_12),
    BUTTON_13(KeyEvent.KEYCODE_BUTTON_13),
    BUTTON_14(KeyEvent.KEYCODE_BUTTON_14),
    BUTTON_15(KeyEvent.KEYCODE_BUTTON_15),
    BUTTON_16(KeyEvent.KEYCODE_BUTTON_16),
    BUTTON_A(KeyEvent.KEYCODE_BUTTON_A),
    BUTTON_B(KeyEvent.KEYCODE_BUTTON_B),
    BUTTON_C(KeyEvent.KEYCODE_BUTTON_C),
    BUTTON_L1(KeyEvent.KEYCODE_BUTTON_L1),
    BUTTON_L2(KeyEvent.KEYCODE_BUTTON_L2),
    BUTTON_MODE(KeyEvent.KEYCODE_BUTTON_MODE),
    BUTTON_R1(KeyEvent.KEYCODE_BUTTON_R1),
    BUTTON_R2(KeyEvent.KEYCODE_BUTTON_R2),
    BUTTON_SELECT(KeyEvent.KEYCODE_BUTTON_SELECT),
    BUTTON_START(KeyEvent.KEYCODE_BUTTON_START),
    BUTTON_THUMBL(KeyEvent.KEYCODE_BUTTON_THUMBL),
    BUTTON_THUMBR(KeyEvent.KEYCODE_BUTTON_THUMBR),
    BUTTON_X(KeyEvent.KEYCODE_BUTTON_X),
    BUTTON_Y(KeyEvent.KEYCODE_BUTTON_Y),
    BUTTON_Z(KeyEvent.KEYCODE_BUTTON_Z),
    DPAD_DOWN(KeyEvent.KEYCODE_DPAD_DOWN),
    DPAD_LEFT(KeyEvent.KEYCODE_DPAD_LEFT),
    DPAD_RIGHT(KeyEvent.KEYCODE_DPAD_RIGHT),
    DPAD_UP(KeyEvent.KEYCODE_DPAD_UP),
}

private val FALLBACK_BUTTONS = arrayOf(
    ButtonInputMapping.BUTTON_1,
    ButtonInputMapping.BUTTON_2,
    ButtonInputMapping.BUTTON_3,
    ButtonInputMapping.BUTTON_4,
    ButtonInputMapping.BUTTON_5,
    ButtonInputMapping.BUTTON_6,
    ButtonInputMapping.BUTTON_7,
    ButtonInputMapping.BUTTON_8,
    ButtonInputMapping.BUTTON_9,
    ButtonInputMapping.BUTTON_10,
    ButtonInputMapping.BUTTON_11,
    ButtonInputMapping.BUTTON_12,
    ButtonInputMapping.BUTTON_13,
    ButtonInputMapping.BUTTON_14,
    ButtonInputMapping.BUTTON_15,
    ButtonInputMapping.BUTTON_16,
)

private enum class AxisInputMapping(val axisCode: Int, val isPositive: Boolean) : InputMapping {
    HAT_X_POS(MotionEvent.AXIS_HAT_X, true),
    HAT_X_NEG(MotionEvent.AXIS_HAT_X, false),
    HAT_Y_POS(MotionEvent.AXIS_HAT_Y, true),
    HAT_Y_NEG(MotionEvent.AXIS_HAT_Y, false),
    RX_POS(MotionEvent.AXIS_RX, true),
    RX_NEG(MotionEvent.AXIS_RX, false),
    RY_POS(MotionEvent.AXIS_RY, true),
    RY_NEG(MotionEvent.AXIS_RY, false),
    RZ_POS(MotionEvent.AXIS_RZ, true),
    RZ_NEG(MotionEvent.AXIS_RZ, false),
    LTRIGGER(MotionEvent.AXIS_LTRIGGER, true),
    RTRIGGER(MotionEvent.AXIS_RTRIGGER, true),
    X_POS(MotionEvent.AXIS_X, true),
    X_NEG(MotionEvent.AXIS_X, false),
    Y_POS(MotionEvent.AXIS_Y, true),
    Y_NEG(MotionEvent.AXIS_Y, false),
    Z_POS(MotionEvent.AXIS_Z, true),
    Z_NEG(MotionEvent.AXIS_Z, false),
}

sealed interface NativeInputButton {
    val nativeKeyCode: Int
}

enum class VPadButtons(override val nativeKeyCode: Int) : NativeInputButton {
    A(NativeInput.VPADButton.A),
    B(NativeInput.VPADButton.B),
    X(NativeInput.VPADButton.X),
    Y(NativeInput.VPADButton.Y),
    L(NativeInput.VPADButton.L),
    R(NativeInput.VPADButton.R),
    ZL(NativeInput.VPADButton.ZL),
    ZR(NativeInput.VPADButton.ZR),
    PLUS(NativeInput.VPADButton.PLUS),
    MINUS(NativeInput.VPADButton.MINUS),
    UP(NativeInput.VPADButton.UP),
    DOWN(NativeInput.VPADButton.DOWN),
    LEFT(NativeInput.VPADButton.LEFT),
    RIGHT(NativeInput.VPADButton.RIGHT),
    STICKL(NativeInput.VPADButton.STICKL),
    STICKR(NativeInput.VPADButton.STICKR),
    STICKL_UP(NativeInput.VPADButton.STICKL_UP),
    STICKL_DOWN(NativeInput.VPADButton.STICKL_DOWN),
    STICKL_LEFT(NativeInput.VPADButton.STICKL_LEFT),
    STICKL_RIGHT(NativeInput.VPADButton.STICKL_RIGHT),
    STICKR_UP(NativeInput.VPADButton.STICKR_UP),
    STICKR_DOWN(NativeInput.VPADButton.STICKR_DOWN),
    STICKR_LEFT(NativeInput.VPADButton.STICKR_LEFT),
    STICKR_RIGHT(NativeInput.VPADButton.STICKR_RIGHT),
    MIC(NativeInput.VPADButton.MIC),
    SCREEN(NativeInput.VPADButton.SCREEN),
    HOME(NativeInput.VPADButton.HOME),
}

enum class ProControllerButtons(override val nativeKeyCode: Int) : NativeInputButton {
    A(NativeInput.ProButton.A),
    B(NativeInput.ProButton.B),
    X(NativeInput.ProButton.X),
    Y(NativeInput.ProButton.Y),
    L(NativeInput.ProButton.L),
    R(NativeInput.ProButton.R),
    ZL(NativeInput.ProButton.ZL),
    ZR(NativeInput.ProButton.ZR),
    PLUS(NativeInput.ProButton.PLUS),
    MINUS(NativeInput.ProButton.MINUS),
    HOME(NativeInput.ProButton.HOME),
    UP(NativeInput.ProButton.UP),
    DOWN(NativeInput.ProButton.DOWN),
    LEFT(NativeInput.ProButton.LEFT),
    RIGHT(NativeInput.ProButton.RIGHT),
    STICKL(NativeInput.ProButton.STICKL),
    STICKR(NativeInput.ProButton.STICKR),
    STICKL_UP(NativeInput.ProButton.STICKL_UP),
    STICKL_DOWN(NativeInput.ProButton.STICKL_DOWN),
    STICKL_LEFT(NativeInput.ProButton.STICKL_LEFT),
    STICKL_RIGHT(NativeInput.ProButton.STICKL_RIGHT),
    STICKR_UP(NativeInput.ProButton.STICKR_UP),
    STICKR_DOWN(NativeInput.ProButton.STICKR_DOWN),
    STICKR_LEFT(NativeInput.ProButton.STICKR_LEFT),
    STICKR_RIGHT(NativeInput.ProButton.STICKR_RIGHT),
}

enum class ClassicControllerButtons(override val nativeKeyCode: Int) : NativeInputButton {
    A(NativeInput.ClassicButton.A),
    B(NativeInput.ClassicButton.B),
    X(NativeInput.ClassicButton.X),
    Y(NativeInput.ClassicButton.Y),
    L(NativeInput.ClassicButton.L),
    R(NativeInput.ClassicButton.R),
    ZL(NativeInput.ClassicButton.ZL),
    ZR(NativeInput.ClassicButton.ZR),
    PLUS(NativeInput.ClassicButton.PLUS),
    MINUS(NativeInput.ClassicButton.MINUS),
    HOME(NativeInput.ClassicButton.HOME),
    UP(NativeInput.ClassicButton.UP),
    DOWN(NativeInput.ClassicButton.DOWN),
    LEFT(NativeInput.ClassicButton.LEFT),
    RIGHT(NativeInput.ClassicButton.RIGHT),
    STICKL_UP(NativeInput.ClassicButton.STICKL_UP),
    STICKL_DOWN(NativeInput.ClassicButton.STICKL_DOWN),
    STICKL_LEFT(NativeInput.ClassicButton.STICKL_LEFT),
    STICKL_RIGHT(NativeInput.ClassicButton.STICKL_RIGHT),
    STICKR_UP(NativeInput.ClassicButton.STICKR_UP),
    STICKR_DOWN(NativeInput.ClassicButton.STICKR_DOWN),
    STICKR_LEFT(NativeInput.ClassicButton.STICKR_LEFT),
    STICKR_RIGHT(NativeInput.ClassicButton.STICKR_RIGHT),
}

enum class WiimoteButtons(override val nativeKeyCode: Int) : NativeInputButton {
    A(NativeInput.WiimoteButton.A),
    B(NativeInput.WiimoteButton.B),
    ONE(NativeInput.WiimoteButton.ONE),
    TWO(NativeInput.WiimoteButton.TWO),
    NUNCHUCK_Z(NativeInput.WiimoteButton.NUNCHUCK_Z),
    NUNCHUCK_C(NativeInput.WiimoteButton.NUNCHUCK_C),
    PLUS(NativeInput.WiimoteButton.PLUS),
    MINUS(NativeInput.WiimoteButton.MINUS),
    UP(NativeInput.WiimoteButton.UP),
    DOWN(NativeInput.WiimoteButton.DOWN),
    LEFT(NativeInput.WiimoteButton.LEFT),
    RIGHT(NativeInput.WiimoteButton.RIGHT),
    NUNCHUCK_UP(NativeInput.WiimoteButton.NUNCHUCK_UP),
    NUNCHUCK_DOWN(NativeInput.WiimoteButton.NUNCHUCK_DOWN),
    NUNCHUCK_LEFT(NativeInput.WiimoteButton.NUNCHUCK_LEFT),
    NUNCHUCK_RIGHT(NativeInput.WiimoteButton.NUNCHUCK_RIGHT),
    HOME(NativeInput.WiimoteButton.HOME),
}

fun getNativeButtonsForControllerType(controllerType: Int): Array<NativeInputButton> {
    return when (controllerType) {
        NativeInput.EmulatedControllerType.VPAD -> VPadButtons.entries.toTypedArray()
        NativeInput.EmulatedControllerType.PRO -> ProControllerButtons.entries.toTypedArray()
        NativeInput.EmulatedControllerType.CLASSIC -> ClassicControllerButtons.entries.toTypedArray()
        NativeInput.EmulatedControllerType.WIIMOTE -> WiimoteButtons.entries.toTypedArray()
        else -> arrayOf()
    }
}
