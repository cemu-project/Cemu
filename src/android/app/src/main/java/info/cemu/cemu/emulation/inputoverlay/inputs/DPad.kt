package info.cemu.cemu.emulation.inputoverlay.inputs

import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Path
import android.graphics.PointF
import android.graphics.Rect
import android.view.MotionEvent
import info.cemu.cemu.emulation.inputoverlay.OverlayDpad
import info.cemu.cemu.emulation.inputoverlay.Colors
import info.cemu.cemu.emulation.inputoverlay.LineBounds
import info.cemu.cemu.emulation.inputoverlay.drawLine
import info.cemu.cemu.emulation.inputoverlay.fillPathWithStroke
import info.cemu.cemu.emulation.inputoverlay.fitInsideRectangle
import info.cemu.cemu.emulation.inputoverlay.rotate
import kotlin.math.atan2
import kotlin.math.min

class DPad(
    private val onButtonStateChange: (button: OverlayDpad, state: Boolean) -> Unit,
    private val alpha: Int,
    rect: Rect,
) : Input(rect) {
    private var backgroundFillColor: Int = 0
    private var backgroundStrokeColor: Int = 0

    private val markerPaint = Paint().apply { strokeWidth = MARKER_WIDTH }
    private val upMarker = LineBounds()
    private val downMarker = LineBounds()
    private val leftMarker = LineBounds()
    private val rightMarker = LineBounds()

    private var dpadState: Int = NONE

    private var centerX: Float = 0f
    private var centerY: Float = 0f
    private var radius2: Float = 0f
    private var radius: Float = 0f
    private var currentPointerId: Int = -1

    private var crossPath = Path()

    override fun configure() {
        backgroundFillColor = Colors.backgroundFill(alpha)
        backgroundStrokeColor = Colors.backgroundStroke(alpha)
        configureColors(alpha)

        centerX = rect.exactCenterX()
        centerY = rect.exactCenterY()
        radius = min(rect.width(), rect.height()) * 0.5f
        radius2 = radius * radius

        val markerSize = radius * 0.35f
        val offset = radius * 0.375f

        fun configureMarker(angleDegrees: Float, lineBounds: LineBounds) {
            val start = PointF(centerX + offset, centerY)
            val stop = PointF(start.x + markerSize, centerY)
            start.rotate(centerX, centerY, angleDegrees)
            stop.rotate(centerX, centerY, angleDegrees)
            lineBounds.setFromPoints(start, stop)
        }


        configureMarker(0f, rightMarker)
        configureMarker(90f, downMarker)
        configureMarker(180f, leftMarker)
        configureMarker(270f, upMarker)

        crossPath = Path(OriginalCrossPath)
        crossPath.fitInsideRectangle(rect, CROSS_PATH_CANVAS_SIZE)
    }

    init {
        configure()
    }

    private fun updateState(nextDpadState: Int) {
        if (nextDpadState == dpadState) return
        updateState(dpadState and nextDpadState.inv(), false)
        updateState(nextDpadState and dpadState.inv(), true)
        dpadState = nextDpadState
    }

    private fun updateState(dpadState: Int, pressed: Boolean) {
        if (dpadState == NONE) return
        if ((dpadState and UP) != 0) {
            onButtonStateChange(OverlayDpad.DPAD_UP, pressed)
        }
        if ((dpadState and DOWN) != 0) {
            onButtonStateChange(OverlayDpad.DPAD_DOWN, pressed)
        }
        if ((dpadState and LEFT) != 0) {
            onButtonStateChange(OverlayDpad.DPAD_LEFT, pressed)
        }
        if ((dpadState and RIGHT) != 0) {
            onButtonStateChange(OverlayDpad.DPAD_RIGHT, pressed)
        }
    }

    private fun getStateByPosition(x: Float, y: Float): Int {
        val dx = x - centerX
        val dy = y - centerY
        val norm2 = dx * dx + dy * dy

        if (norm2 <= radius2 * 0.1f) {
            return NONE
        }

        val angle = atan2(dy, dx)

        return when (angle) {
            in -0.875 * Math.PI..<-0.625 * Math.PI -> UP or LEFT
            in -0.625 * Math.PI..<-0.375 * Math.PI -> UP
            in -0.375 * Math.PI..<-0.125 * Math.PI -> UP or RIGHT
            in -0.125 * Math.PI..<0.125 * Math.PI -> RIGHT
            in 0.125 * Math.PI..<0.375 * Math.PI -> DOWN or RIGHT
            in 0.375 * Math.PI..<0.625 * Math.PI -> DOWN
            in 0.625 * Math.PI..<0.875 * Math.PI -> DOWN or LEFT
            else -> LEFT // in [-pi, -0.875*pi) or [0.875*pi, pi]
        }
    }

    override fun onTouch(event: MotionEvent): Boolean {
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                if (currentPointerId != -1) {
                    return false
                }
                val pointerIndex = event.actionIndex
                val x = event.getX(pointerIndex)
                val y = event.getY(pointerIndex)
                val pointerId = event.getPointerId(pointerIndex)
                if (isInside(x, y)) {
                    currentPointerId = pointerId
                    updateState(getStateByPosition(x, y))
                    return true
                }
            }

            MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP -> {
                val pointerId = event.getPointerId(event.actionIndex)
                if (pointerId == currentPointerId) {
                    currentPointerId = -1
                    updateState(NONE)
                    return true
                }
            }

            MotionEvent.ACTION_MOVE -> {
                if (currentPointerId == -1) {
                    return false
                }
                for (i in 0 until event.pointerCount) {
                    if (currentPointerId != event.getPointerId(i)) {
                        continue
                    }
                    val x = event.getX(i)
                    val y = event.getY(i)
                    updateState(getStateByPosition(x, y))
                    return true
                }
            }
        }
        return false
    }

    override fun resetInput() {
        updateState(NONE)
    }

    override fun isInside(x: Float, y: Float): Boolean {
        val dx = x - centerX
        val dy = y - centerY
        return dx * dx + dy * dy <= radius2
    }

    override fun drawInput(canvas: Canvas) {
        canvas.fillPathWithStroke(crossPath, paint, backgroundFillColor, backgroundStrokeColor)

        drawMarker(canvas, upMarker, dpadState and UP)
        drawMarker(canvas, downMarker, dpadState and DOWN)
        drawMarker(canvas, leftMarker, dpadState and LEFT)
        drawMarker(canvas, rightMarker, dpadState and RIGHT)
    }

    private fun drawMarker(canvas: Canvas, bounds: LineBounds, state: Int) {
        if (state != 0) {
            markerPaint.color = activeStrokeColor
        } else {
            markerPaint.color = inactiveStrokeColor
        }

        canvas.drawLine(bounds, markerPaint)
    }

    companion object {
        private const val CROSS_PATH_CANVAS_SIZE = 100F
        private val OriginalCrossPath by lazy(LazyThreadSafetyMode.NONE) {
            Path().apply {
                moveTo(0f, -65f)
                lineTo(35f, -65f)
                lineTo(35f, -100f)
                rLineTo(30f, 0f)
                rLineTo(0f, 35f)
                rLineTo(35f, 0f)
                lineTo(100f, -35f)
                lineTo(65f, -35f)
                rLineTo(0f, 35f)
                lineTo(35f, 0f)
                lineTo(35f, -35f)
                lineTo(0f, -35f)
                close()
            }
        }
        private const val MARKER_WIDTH = 15f
        private const val NONE = 0
        private const val UP = 1 shl 0
        private const val DOWN = 1 shl 1
        private const val LEFT = 1 shl 2
        private const val RIGHT = 1 shl 3
    }
}
