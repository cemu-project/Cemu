package info.cemu.cemu.emulation.inputoverlay

import info.cemu.cemu.common.settings.OverlayInputConfig

sealed interface OverlayInput

enum class OverlayJoystick : OverlayInput {
    LEFT,
    RIGHT;
}

enum class OverlayButton : OverlayInput {
    A,
    B,
    ONE,
    TWO,
    C,
    Z,
    HOME,
    L,
    L_STICK_CLICK,
    MINUS,
    PLUS,
    R,
    R_STICK_CLICK,
    X,
    Y,
    ZL,
    ZR,
    BLOW_MIC;
}

enum class OverlayDpad : OverlayInput {
    DPAD_DOWN,
    DPAD_LEFT,
    DPAD_RIGHT,
    DPAD_UP;
}

fun OverlayInput.toConfig(): OverlayInputConfig = when (this) {
    is OverlayButton -> when (this) {
        OverlayButton.A -> OverlayInputConfig.BUTTON_A
        OverlayButton.B -> OverlayInputConfig.BUTTON_B
        OverlayButton.ONE -> OverlayInputConfig.BUTTON_ONE
        OverlayButton.TWO -> OverlayInputConfig.BUTTON_TWO
        OverlayButton.C -> OverlayInputConfig.BUTTON_C
        OverlayButton.Z -> OverlayInputConfig.BUTTON_Z
        OverlayButton.HOME -> OverlayInputConfig.BUTTON_HOME
        OverlayButton.L -> OverlayInputConfig.BUTTON_L
        OverlayButton.L_STICK_CLICK -> OverlayInputConfig.BUTTON_L_STICK_CLICK
        OverlayButton.MINUS -> OverlayInputConfig.BUTTON_MINUS
        OverlayButton.PLUS -> OverlayInputConfig.BUTTON_PLUS
        OverlayButton.R -> OverlayInputConfig.BUTTON_R
        OverlayButton.R_STICK_CLICK -> OverlayInputConfig.BUTTON_R_STICK_CLICK
        OverlayButton.X -> OverlayInputConfig.BUTTON_X
        OverlayButton.Y -> OverlayInputConfig.BUTTON_Y
        OverlayButton.ZL -> OverlayInputConfig.BUTTON_ZL
        OverlayButton.ZR -> OverlayInputConfig.BUTTON_ZR
        OverlayButton.BLOW_MIC -> OverlayInputConfig.BUTTON_BLOW_MIC
    }

    is OverlayJoystick -> when (this) {
        OverlayJoystick.LEFT -> OverlayInputConfig.JOYSTICK_LEFT
        OverlayJoystick.RIGHT -> OverlayInputConfig.JOYSTICK_RIGHT
    }

    is OverlayDpad -> OverlayInputConfig.DPAD
}
