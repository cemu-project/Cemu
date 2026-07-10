package info.cemu.cemu.emulation.inputoverlay

import android.content.Context
import android.graphics.Canvas
import android.graphics.Rect
import android.os.VibrationEffect
import android.os.Vibrator
import android.util.DisplayMetrics
import android.view.MotionEvent
import android.view.SurfaceView
import android.view.View
import android.view.View.OnTouchListener
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.viewinterop.AndroidView
import info.cemu.cemu.R
import info.cemu.cemu.common.android.context.getDeviceVibrator
import info.cemu.cemu.common.settings.InputOverlayRect
import info.cemu.cemu.common.settings.InputOverlaySettings
import info.cemu.cemu.common.settings.OverlayInputConfig
import info.cemu.cemu.common.settings.getDefaultRectangle
import info.cemu.cemu.emulation.inputoverlay.inputs.DPad
import info.cemu.cemu.emulation.inputoverlay.inputs.Input
import info.cemu.cemu.emulation.inputoverlay.inputs.Joystick
import info.cemu.cemu.emulation.inputoverlay.inputs.RectangleButton
import info.cemu.cemu.emulation.inputoverlay.inputs.RoundButton
import info.cemu.cemu.emulation.inputoverlay.inputs.innerdrawing.BlowButtonInnerDrawing
import info.cemu.cemu.emulation.inputoverlay.inputs.innerdrawing.ButtonInnerDrawing
import info.cemu.cemu.emulation.inputoverlay.inputs.innerdrawing.HomeButtonInnerDrawing
import info.cemu.cemu.emulation.inputoverlay.inputs.innerdrawing.TextButtonInnerDrawing
import info.cemu.cemu.nativeinterface.NativeInput
import info.cemu.cemu.nativeinterface.NativeInput.getControllerType
import info.cemu.cemu.nativeinterface.NativeInput.isControllerDisabled
import info.cemu.cemu.nativeinterface.NativeInput.onOverlayAxis
import info.cemu.cemu.nativeinterface.NativeInput.onOverlayButton
import kotlin.math.roundToInt

class InputOverlaySurfaceView(context: Context) : SurfaceView(context), OnTouchListener {
    enum class InputMode {
        DEFAULT, EDIT_POSITION, EDIT_SIZE,
    }

    var onEditFinishedListener: ((Map<OverlayInputConfig, InputOverlayRect>) -> Unit)? =
        null

    private var inputMode = InputMode.DEFAULT
    private var pixelDensity = 1
    private var currentConfiguredInput: Input? = null
    private var nativeControllerType = NativeInput.EmulatedControllerType.DISABLED
    private var onJoystickChange: (OverlayInput, Float, Float, Float, Float) -> Unit =
        { _, _, _, _, _ -> }
    private var overlyButtonToNativeButton: (OverlayInput) -> Int = { _ -> -1 }
    private var inputs: MutableList<Pair<OverlayInput, Input>> = mutableListOf()
    private val vibrator: Vibrator = context.getDeviceVibrator()
    private val buttonTouchVibrationEffect =
        VibrationEffect.createPredefined(VibrationEffect.EFFECT_TICK)
    private val inputsMinSize: Int
    private val isVisible: Boolean
        get() = visibility == VISIBLE

    private var settings: InputOverlaySettings = InputOverlaySettings()
    private val controllerIndex get() = settings.controllerIndex
    private val currentAlpha get() = settings.alpha
    private val isVibrateOnTouchEnabled: Boolean
        get() = settings.isVibrateOnTouchEnabled && vibrator.hasVibrator()

    init {
        visibility = GONE

        pixelDensity = context.resources.displayMetrics.densityDpi

        inputsMinSize =
            (INPUTS_MIN_SIZE_DP * pixelDensity.toFloat() / DisplayMetrics.DENSITY_DEFAULT).roundToInt()

        setOnTouchListener(this)
    }

    fun applySettings(inputOverlaySettings: InputOverlaySettings) {
        if (this.settings == inputOverlaySettings) {
            return
        }

        this.settings = inputOverlaySettings

        setInputs()

        invalidate()
    }

    fun setVisible(isVisible: Boolean) {
        if (this.isVisible == isVisible) {
            return
        }

        visibility = if (isVisible) VISIBLE else GONE

        invalidate()
    }

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        if (oldw == 0 || oldh == 0) {
            setInputs()
        }

        super.onSizeChanged(w, h, oldw, oldh)
    }

    fun setInputMode(inputMode: InputMode) {
        if (this.inputMode == inputMode) {
            return
        }

        this.inputMode = inputMode

        if (this.inputMode != InputMode.DEFAULT) {
            return
        }

        val inputsRectangles =
            inputs.associate { it.first.toConfig() to it.second.getBoundingRectangle() }

        onEditFinishedListener?.invoke(inputsRectangles)
    }

    private fun overlayButtonToVPADButton(button: OverlayInput): Int {
        return when (button) {
            OverlayButton.A -> NativeInput.VPADButton.A
            OverlayButton.B -> NativeInput.VPADButton.B
            OverlayButton.X -> NativeInput.VPADButton.X
            OverlayButton.Y -> NativeInput.VPADButton.Y
            OverlayButton.PLUS -> NativeInput.VPADButton.PLUS
            OverlayButton.MINUS -> NativeInput.VPADButton.MINUS
            OverlayDpad.DPAD_UP -> NativeInput.VPADButton.UP
            OverlayDpad.DPAD_DOWN -> NativeInput.VPADButton.DOWN
            OverlayDpad.DPAD_LEFT -> NativeInput.VPADButton.LEFT
            OverlayDpad.DPAD_RIGHT -> NativeInput.VPADButton.RIGHT
            OverlayButton.L_STICK_CLICK -> NativeInput.VPADButton.STICKL
            OverlayButton.R_STICK_CLICK -> NativeInput.VPADButton.STICKR
            OverlayButton.L -> NativeInput.VPADButton.L
            OverlayButton.R -> NativeInput.VPADButton.R
            OverlayButton.ZR -> NativeInput.VPADButton.ZR
            OverlayButton.ZL -> NativeInput.VPADButton.ZL
            OverlayButton.BLOW_MIC -> NativeInput.VPADButton.MIC
            else -> -1
        }
    }

    private fun overlayButtonToClassicButton(button: OverlayInput): Int {
        return when (button) {
            OverlayButton.A -> NativeInput.ClassicButton.A
            OverlayButton.B -> NativeInput.ClassicButton.B
            OverlayButton.X -> NativeInput.ClassicButton.X
            OverlayButton.Y -> NativeInput.ClassicButton.Y
            OverlayButton.PLUS -> NativeInput.ClassicButton.PLUS
            OverlayButton.MINUS -> NativeInput.ClassicButton.MINUS
            OverlayDpad.DPAD_UP -> NativeInput.ClassicButton.UP
            OverlayDpad.DPAD_DOWN -> NativeInput.ClassicButton.DOWN
            OverlayDpad.DPAD_LEFT -> NativeInput.ClassicButton.LEFT
            OverlayDpad.DPAD_RIGHT -> NativeInput.ClassicButton.RIGHT
            OverlayButton.L -> NativeInput.ClassicButton.L
            OverlayButton.R -> NativeInput.ClassicButton.R
            OverlayButton.ZR -> NativeInput.ClassicButton.ZR
            OverlayButton.ZL -> NativeInput.ClassicButton.ZL
            else -> -1
        }
    }

    private fun overlayButtonToProButton(button: OverlayInput): Int {
        return when (button) {
            OverlayButton.A -> NativeInput.ProButton.A
            OverlayButton.B -> NativeInput.ProButton.B
            OverlayButton.X -> NativeInput.ProButton.X
            OverlayButton.Y -> NativeInput.ProButton.Y
            OverlayButton.PLUS -> NativeInput.ProButton.PLUS
            OverlayButton.MINUS -> NativeInput.ProButton.MINUS
            OverlayDpad.DPAD_UP -> NativeInput.ProButton.UP
            OverlayDpad.DPAD_DOWN -> NativeInput.ProButton.DOWN
            OverlayDpad.DPAD_LEFT -> NativeInput.ProButton.LEFT
            OverlayDpad.DPAD_RIGHT -> NativeInput.ProButton.RIGHT
            OverlayButton.L_STICK_CLICK -> NativeInput.ProButton.STICKL
            OverlayButton.R_STICK_CLICK -> NativeInput.ProButton.STICKR
            OverlayButton.L -> NativeInput.ProButton.L
            OverlayButton.R -> NativeInput.ProButton.R
            OverlayButton.ZR -> NativeInput.ProButton.ZR
            OverlayButton.ZL -> NativeInput.ProButton.ZL
            else -> -1
        }
    }

    private fun overlayButtonToWiimoteButton(button: OverlayInput): Int {
        return when (button) {
            OverlayButton.A -> NativeInput.WiimoteButton.A
            OverlayButton.B -> NativeInput.WiimoteButton.B
            OverlayButton.ONE -> NativeInput.WiimoteButton.ONE
            OverlayButton.TWO -> NativeInput.WiimoteButton.TWO
            OverlayButton.PLUS -> NativeInput.WiimoteButton.PLUS
            OverlayButton.MINUS -> NativeInput.WiimoteButton.MINUS
            OverlayButton.HOME -> NativeInput.WiimoteButton.HOME
            OverlayDpad.DPAD_UP -> NativeInput.WiimoteButton.UP
            OverlayDpad.DPAD_DOWN -> NativeInput.WiimoteButton.DOWN
            OverlayDpad.DPAD_LEFT -> NativeInput.WiimoteButton.LEFT
            OverlayDpad.DPAD_RIGHT -> NativeInput.WiimoteButton.RIGHT
            OverlayButton.C -> NativeInput.WiimoteButton.NUNCHUCK_C
            OverlayButton.Z -> NativeInput.WiimoteButton.NUNCHUCK_Z
            else -> -1
        }
    }

    private fun onButtonStateChange(button: OverlayInput, state: Boolean) {
        val nativeButtonId = overlyButtonToNativeButton(button)

        if (nativeButtonId == -1) {
            return
        }

        if (isVibrateOnTouchEnabled && state) {
            vibrator.vibrate(buttonTouchVibrationEffect)
        }

        onOverlayButton(controllerIndex, nativeButtonId, state)
    }

    private fun onOverlayAxis(axis: Int, value: Float) {
        onOverlayAxis(controllerIndex, axis, value)
    }

    private fun onVPADJoystickStateChange(
        joystick: OverlayInput,
        up: Float,
        down: Float,
        left: Float,
        right: Float,
    ) {
        if (joystick == OverlayJoystick.LEFT) {
            onOverlayAxis(NativeInput.VPADButton.STICKL_UP, up)
            onOverlayAxis(NativeInput.VPADButton.STICKL_DOWN, down)
            onOverlayAxis(NativeInput.VPADButton.STICKL_LEFT, left)
            onOverlayAxis(NativeInput.VPADButton.STICKL_RIGHT, right)
        } else if (joystick == OverlayJoystick.RIGHT) {
            onOverlayAxis(NativeInput.VPADButton.STICKR_UP, up)
            onOverlayAxis(NativeInput.VPADButton.STICKR_DOWN, down)
            onOverlayAxis(NativeInput.VPADButton.STICKR_LEFT, left)
            onOverlayAxis(NativeInput.VPADButton.STICKR_RIGHT, right)
        }
    }

    private fun onProJoystickStateChange(
        joystick: OverlayInput,
        up: Float,
        down: Float,
        left: Float,
        right: Float,
    ) {
        if (joystick == OverlayJoystick.LEFT) {
            onOverlayAxis(NativeInput.ProButton.STICKL_UP, up)
            onOverlayAxis(NativeInput.ProButton.STICKL_DOWN, down)
            onOverlayAxis(NativeInput.ProButton.STICKL_LEFT, left)
            onOverlayAxis(NativeInput.ProButton.STICKL_RIGHT, right)
        } else if (joystick == OverlayJoystick.RIGHT) {
            onOverlayAxis(NativeInput.ProButton.STICKR_UP, up)
            onOverlayAxis(NativeInput.ProButton.STICKR_DOWN, down)
            onOverlayAxis(NativeInput.ProButton.STICKR_LEFT, left)
            onOverlayAxis(NativeInput.ProButton.STICKR_RIGHT, right)
        }
    }

    private fun onClassicJoystickStateChange(
        joystick: OverlayInput,
        up: Float,
        down: Float,
        left: Float,
        right: Float,
    ) {
        if (joystick == OverlayJoystick.LEFT) {
            onOverlayAxis(NativeInput.ClassicButton.STICKL_UP, up)
            onOverlayAxis(NativeInput.ClassicButton.STICKL_DOWN, down)
            onOverlayAxis(NativeInput.ClassicButton.STICKL_LEFT, left)
            onOverlayAxis(NativeInput.ClassicButton.STICKL_RIGHT, right)
        } else if (joystick == OverlayJoystick.RIGHT) {
            onOverlayAxis(NativeInput.ClassicButton.STICKR_UP, up)
            onOverlayAxis(NativeInput.ClassicButton.STICKR_DOWN, down)
            onOverlayAxis(NativeInput.ClassicButton.STICKR_LEFT, left)
            onOverlayAxis(NativeInput.ClassicButton.STICKR_RIGHT, right)
        }
    }

    private fun onWiimoteJoystickStateChange(
        joystick: OverlayInput,
        up: Float,
        down: Float,
        left: Float,
        right: Float,
    ) {
        if (joystick == OverlayJoystick.RIGHT) {
            onOverlayAxis(NativeInput.WiimoteButton.NUNCHUCK_UP, up)
            onOverlayAxis(NativeInput.WiimoteButton.NUNCHUCK_DOWN, down)
            onOverlayAxis(NativeInput.WiimoteButton.NUNCHUCK_LEFT, left)
            onOverlayAxis(NativeInput.WiimoteButton.NUNCHUCK_RIGHT, right)
        }
    }

    private fun onJoystickStateChange(joystick: OverlayInput, x: Float, y: Float) {
        val (up, down) = if (y < 0) Pair(-y, 0f) else Pair(0f, y)
        val (left, right) = if (x < 0) Pair(-x, 0f) else Pair(0f, x)
        onJoystickChange(joystick, up, down, left, right)
    }

    private fun getBoundingRectangleForInput(input: OverlayInput): Rect {
        val rect = settings.inputOverlayRectMap[input.toConfig()]

        if (rect != null) {
            return Rect(
                rect.left,
                rect.top,
                rect.right,
                rect.bottom
            )
        }

        return getDefaultRectangle(input.toConfig(), width, height, pixelDensity)
    }

    private fun MutableList<Pair<OverlayInput, Input>>.addRoundButton(
        button: OverlayButton,
        innerDrawing: ButtonInnerDrawing,
    ) {
        add(
            button to RoundButton(
                innerDrawing,
                { onButtonStateChange(button, it) },
                currentAlpha,
                getBoundingRectangleForInput(button),
            )
        )
    }

    private fun MutableList<Pair<OverlayInput, Input>>.addRoundButton(
        button: OverlayButton,
        buttonText: String = button.name,
    ) = addRoundButton(button, TextButtonInnerDrawing(buttonText))

    private fun MutableList<Pair<OverlayInput, Input>>.addJoystick(joystick: OverlayJoystick) {
        add(
            joystick to Joystick(
                { x, y -> onJoystickStateChange(joystick, x, y) },
                currentAlpha,
                getBoundingRectangleForInput(joystick)
            )
        )
    }

    private fun MutableList<Pair<OverlayInput, Input>>.addDpad() {
        add(
            OverlayDpad.DPAD_UP to DPad(
                ::onButtonStateChange,
                currentAlpha,
                getBoundingRectangleForInput(OverlayDpad.DPAD_UP)
            )
        )
    }

    private fun MutableList<Pair<OverlayInput, Input>>.addRectangleButton(
        button: OverlayButton,
        buttonText: String = button.name,
    ) {
        add(
            button to RectangleButton(
                TextButtonInnerDrawing(buttonText),
                { onButtonStateChange(button, it) },
                currentAlpha,
                getBoundingRectangleForInput(button)
            )
        )
    }

    private fun setInputs() {
        if (isControllerDisabled(controllerIndex)) {
            inputs = mutableListOf()
            return
        }

        nativeControllerType = getControllerType(controllerIndex)
        overlyButtonToNativeButton = when (nativeControllerType) {
            NativeInput.EmulatedControllerType.VPAD -> ::overlayButtonToVPADButton
            NativeInput.EmulatedControllerType.CLASSIC -> ::overlayButtonToClassicButton
            NativeInput.EmulatedControllerType.PRO -> ::overlayButtonToProButton
            NativeInput.EmulatedControllerType.WIIMOTE -> ::overlayButtonToWiimoteButton
            else -> { _ -> -1 }
        }
        onJoystickChange = when (nativeControllerType) {
            NativeInput.EmulatedControllerType.VPAD -> ::onVPADJoystickStateChange
            NativeInput.EmulatedControllerType.PRO -> ::onProJoystickStateChange
            NativeInput.EmulatedControllerType.CLASSIC -> ::onClassicJoystickStateChange
            NativeInput.EmulatedControllerType.WIIMOTE -> ::onWiimoteJoystickStateChange
            else -> { _, _, _, _, _ -> }
        }

        inputs = mutableListOf<Pair<OverlayInput, Input>>().apply {
            addRoundButton(OverlayButton.MINUS, "-")
            addRoundButton(OverlayButton.PLUS, "+")
            addDpad()
            addRoundButton(OverlayButton.A)
            addRoundButton(OverlayButton.B)
            addJoystick(OverlayJoystick.RIGHT)
            if (nativeControllerType != NativeInput.EmulatedControllerType.WIIMOTE) {
                addRoundButton(OverlayButton.X)
                addRoundButton(OverlayButton.Y)
                addRectangleButton(OverlayButton.ZL)
                addRectangleButton(OverlayButton.ZR)
                addRectangleButton(OverlayButton.L)
                addRectangleButton(OverlayButton.R)
                addJoystick(OverlayJoystick.LEFT)
            }
            if (nativeControllerType == NativeInput.EmulatedControllerType.WIIMOTE) {
                addRoundButton(OverlayButton.ONE, "1")
                addRoundButton(OverlayButton.TWO, "2")
                addRoundButton(OverlayButton.C)
                addRectangleButton(OverlayButton.Z)
                addRoundButton(OverlayButton.HOME, HomeButtonInnerDrawing())
            }
            if (nativeControllerType != NativeInput.EmulatedControllerType.CLASSIC && nativeControllerType != NativeInput.EmulatedControllerType.WIIMOTE) {
                addRoundButton(OverlayButton.L_STICK_CLICK, "L3")
                addRoundButton(OverlayButton.R_STICK_CLICK, "R3")
            }

            if (nativeControllerType == NativeInput.EmulatedControllerType.VPAD) {
                addRoundButton(OverlayButton.BLOW_MIC, BlowButtonInnerDrawing())
            }

            removeAll { (overlayInput, _) -> !isInputVisible(overlayInput) }
        }
    }

    private fun isInputVisible(overlayInput: OverlayInput) =
        settings.inputVisibilityMap[overlayInput.toConfig()] ?: true

    override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
        super.onLayout(changed, left, top, right, bottom)
        setWillNotDraw(false)
        requestFocus()
    }

    override fun draw(canvas: Canvas) {
        super.draw(canvas)

        for ((_, input) in inputs) {
            input.draw(canvas)
        }
    }

    private fun onEditPosition(event: MotionEvent): Boolean {
        val configuredInput = currentConfiguredInput
        if (event.actionMasked == MotionEvent.ACTION_DOWN) {
            if (configuredInput != null) {
                return false
            }
            val x = event.x
            val y = event.y
            for ((_, input) in inputs) {
                if (input.isInside(x, y)) {
                    currentConfiguredInput = input
                    input.enableDrawingBoundingRect(
                        resources.getColor(R.color.purple, context.theme)
                    )
                    val x = event.x.toInt()
                    val y = event.y.toInt()
                    input.moveInput(x, y, width, height)
                    return true
                }
            }
        }

        if (configuredInput == null) {
            return false
        }

        if (event.actionMasked == MotionEvent.ACTION_UP) {
            configuredInput.disableDrawingBoundingRect()
            currentConfiguredInput = null
            return true
        }

        if (event.actionMasked == MotionEvent.ACTION_MOVE) {
            val x = event.x.toInt()
            val y = event.y.toInt()
            configuredInput.moveInput(x, y, width, height)
            return true
        }

        return false
    }

    private fun onEditSize(event: MotionEvent): Boolean {
        val configuredInput = currentConfiguredInput

        if (event.actionMasked == MotionEvent.ACTION_DOWN) {
            val x = event.x
            val y = event.y
            for ((_, input) in inputs) {
                if (input.isInside(x, y)) {
                    currentConfiguredInput = input
                    val color = resources.getColor(R.color.red, context.theme)
                    input.enableDrawingBoundingRect(color)
                    return true
                }
            }
        }

        if (configuredInput == null) {
            return false
        }

        if (event.actionMasked == MotionEvent.ACTION_UP) {
            configuredInput.disableDrawingBoundingRect()
            currentConfiguredInput = null
            return true
        }

        if (event.actionMasked == MotionEvent.ACTION_MOVE) {
            val histSize = event.historySize
            if (event.historySize >= 2) {
                val x1 = event.getHistoricalX(0)
                val y1 = event.getHistoricalY(0)
                val x2 = event.getHistoricalX(histSize - 1)
                val y2 = event.getHistoricalY(histSize - 1)
                configuredInput.resize(
                    diffX = (x2 - x1).toInt(),
                    diffY = (y2 - y1).toInt(),
                    maxWidth = width,
                    maxHeight = height,
                    minWidthHeight = inputsMinSize
                )
            }
            return true
        }
        return false
    }

    override fun onTouch(v: View, event: MotionEvent): Boolean {
        var touchEventProcessed = false
        when (inputMode) {
            InputMode.DEFAULT -> {
                for ((_, input) in inputs) {
                    if (input.onTouch(event)) {
                        touchEventProcessed = true
                    }
                }
            }

            InputMode.EDIT_POSITION -> touchEventProcessed = onEditPosition(event)
            InputMode.EDIT_SIZE -> touchEventProcessed = onEditSize(event)
        }

        if (touchEventProcessed) {
            invalidate()
        }

        return touchEventProcessed
    }

    companion object {
        private const val INPUTS_MIN_SIZE_DP = 20
    }
}

@Composable
fun InputOverlaySurface(
    isVisible: Boolean,
    inputOverlaySettings: InputOverlaySettings,
    inputMode: InputOverlaySurfaceView.InputMode,
    onEditFinished: (Map<OverlayInputConfig, InputOverlayRect>) -> Unit,
) {
    AndroidView(
        modifier = Modifier.fillMaxSize(),
        factory = { context ->
            InputOverlaySurfaceView(context).apply {
                setVisible(isVisible)
                setInputMode(inputMode)
                applySettings(inputOverlaySettings)
                onEditFinishedListener = onEditFinished
            }
        },
        update = { view ->
            view.setVisible(isVisible)
            view.setInputMode(inputMode)
            view.applySettings(inputOverlaySettings)
            view.onEditFinishedListener = onEditFinished
        }
    )
}