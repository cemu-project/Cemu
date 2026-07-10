package info.cemu.cemu.common.settings

import android.graphics.Rect
import android.util.DisplayMetrics.DENSITY_DEFAULT
import kotlin.math.max
import kotlin.math.min

data class InputOverlayConfig(
    val alignBottom: Boolean = false,
    val alignEnd: Boolean = false,
    val paddingHorizontal: Int = 0,
    val paddingVertical: Int = 0,
    val width: Int,
    val height: Int
) {
    constructor(
        alignBottom: Boolean = false,
        alignEnd: Boolean = false,
        paddingHorizontal: Int = 0,
        paddingVertical: Int = 0,
        size: Int
    ) : this(
        alignBottom = alignBottom,
        alignEnd = alignEnd,
        paddingHorizontal = paddingHorizontal,
        paddingVertical = paddingVertical,
        width = size,
        height = size
    )
}

fun defaultOverlayConfigFor(input: OverlayInputConfig): InputOverlayConfig {
    return when (input) {
        OverlayInputConfig.BUTTON_PLUS -> InputOverlayConfig(
            alignBottom = true,
            alignEnd = true,
            paddingHorizontal = 272,
            paddingVertical = 24,
            size = 48
        )

        OverlayInputConfig.BUTTON_MINUS -> InputOverlayConfig(
            alignBottom = true,
            paddingHorizontal = 272,
            paddingVertical = 24,
            size = 48
        )

        OverlayInputConfig.BUTTON_HOME -> InputOverlayConfig(
            alignBottom = true,
            alignEnd = true,
            paddingHorizontal = 208,
            paddingVertical = 80,
            size = 48
        )

        OverlayInputConfig.BUTTON_L -> InputOverlayConfig(
            paddingHorizontal = 48,
            paddingVertical = 8,
            width = 72,
            height = 36
        )

        OverlayInputConfig.BUTTON_ZL -> InputOverlayConfig(
            paddingHorizontal = 48,
            paddingVertical = 64,
            width = 72,
            height = 36
        )

        OverlayInputConfig.BUTTON_R -> InputOverlayConfig(
            alignEnd = true,
            paddingHorizontal = 48,
            paddingVertical = 8,
            width = 72,
            height = 36
        )

        OverlayInputConfig.BUTTON_ZR -> InputOverlayConfig(
            alignEnd = true,
            paddingHorizontal = 48,
            paddingVertical = 64,
            width = 72,
            height = 36
        )

        OverlayInputConfig.BUTTON_C -> InputOverlayConfig(
            alignEnd = true,
            paddingHorizontal = 60,
            paddingVertical = 8,
            size = 48
        )

        OverlayInputConfig.BUTTON_Z -> InputOverlayConfig(
            alignEnd = true,
            paddingHorizontal = 48,
            paddingVertical = 64,
            width = 72,
            height = 36
        )

        OverlayInputConfig.BUTTON_A -> InputOverlayConfig(
            alignBottom = true,
            alignEnd = true,
            paddingHorizontal = 32,
            paddingVertical = 56,
            size = 48
        )

        OverlayInputConfig.BUTTON_Y -> InputOverlayConfig(
            alignBottom = true,
            alignEnd = true,
            paddingHorizontal = 128,
            paddingVertical = 56,
            size = 48
        )

        OverlayInputConfig.BUTTON_X -> InputOverlayConfig(
            alignBottom = true,
            alignEnd = true,
            paddingHorizontal = 80,
            paddingVertical = 104,
            size = 48
        )

        OverlayInputConfig.BUTTON_B -> InputOverlayConfig(
            alignBottom = true,
            alignEnd = true,
            paddingHorizontal = 80,
            paddingVertical = 8,
            size = 48
        )

        OverlayInputConfig.BUTTON_ONE -> InputOverlayConfig(
            alignBottom = true,
            alignEnd = true,
            paddingHorizontal = 128,
            paddingVertical = 56,
            size = 48
        )

        OverlayInputConfig.BUTTON_TWO -> InputOverlayConfig(
            alignBottom = true,
            alignEnd = true,
            paddingHorizontal = 80,
            paddingVertical = 104,
            size = 48
        )

        OverlayInputConfig.BUTTON_R_STICK_CLICK -> InputOverlayConfig(
            alignBottom = true,
            alignEnd = true,
            paddingHorizontal = 208,
            paddingVertical = 80,
            size = 48
        )

        OverlayInputConfig.BUTTON_L_STICK_CLICK -> InputOverlayConfig(
            alignBottom = true,
            paddingHorizontal = 208,
            paddingVertical = 80,
            size = 48
        )

        OverlayInputConfig.JOYSTICK_LEFT -> InputOverlayConfig(
            alignBottom = true,
            paddingHorizontal = 144,
            paddingVertical = 120,
            size = 72
        )

        OverlayInputConfig.JOYSTICK_RIGHT -> InputOverlayConfig(
            alignEnd = true,
            alignBottom = true,
            paddingHorizontal = 144,
            paddingVertical = 120,
            size = 72
        )

        OverlayInputConfig.DPAD -> InputOverlayConfig(
            alignBottom = true,
            size = 144,
            paddingHorizontal = 8,
            paddingVertical = 8
        )

        OverlayInputConfig.BUTTON_BLOW_MIC -> InputOverlayConfig(
            alignBottom = true,
            alignEnd = true,
            paddingHorizontal = 8,
            paddingVertical = 8,
            size = 40
        )
    }
}

fun getDefaultRectangle(
    input: OverlayInputConfig,
    width: Int,
    height: Int,
    density: Int,
): Rect {
    fun Int.dpToPx() = (this * density) / DENSITY_DEFAULT
    val inputConfig = defaultOverlayConfigFor(input)
    val inputWidth = inputConfig.width.dpToPx()
    val horizontalPadding = inputConfig.paddingHorizontal.dpToPx()
    val verticalPadding = inputConfig.paddingVertical.dpToPx()
    val inputHeight = inputConfig.height.dpToPx()
    val top = min(
        max(if (inputConfig.alignBottom) height - inputHeight else 0, verticalPadding),
        height - verticalPadding - inputHeight
    )
    val left = min(
        max(if (inputConfig.alignEnd) width - inputWidth else 0, horizontalPadding),
        width - horizontalPadding - inputWidth
    )
    val right = left + inputWidth
    val bottom = top + inputHeight
    return Rect(
        left,
        top,
        right,
        bottom,
    )
}
