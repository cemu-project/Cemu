package info.cemu.cemu.emulation.inputoverlay.inputs

import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Rect
import android.view.MotionEvent
import info.cemu.cemu.emulation.inputoverlay.Colors
import info.cemu.cemu.emulation.inputoverlay.fillCircleWithStroke
import kotlin.math.min
import kotlin.math.sqrt

class Joystick(
    private val onStickStateChange: (x: Float, y: Float) -> Unit,
    private val alpha: Int,
    boundingRectangle: Rect,
) : Input(boundingRectangle) {
    private var backgroundFillColor: Int = 0
    private var currentPointerId = -1
    private var pressed = false
    private var centerX: Float = 0f
    private var centerY: Float = 0f
    private var originalCenterX: Float = 0f
    private var originalCenterY: Float = 0f
    private var radius: Float = 0f
    private var innerRadius: Float = 0f
    private var innerCenterX: Float = 0f
    private var innerCenterY: Float = 0f

    init {
        configure()
    }

    private fun updateState(pressed: Boolean, x: Float, y: Float) {
        this.pressed = pressed
        onStickStateChange(x, y)
        innerCenterX = centerX + radius * x
        innerCenterY = centerY + radius * y
    }

    override fun configure() {
        backgroundFillColor = Colors.backgroundFill(alpha)
        configureColors(alpha)

        centerX = rect.exactCenterX()
        originalCenterX = centerX
        innerCenterX = centerX
        centerY = rect.exactCenterY()
        originalCenterY = centerY
        innerCenterY = centerY
        radius = min(rect.width(), rect.height()) * 0.5f
        innerRadius = radius * 0.65f
    }

    override fun onTouch(event: MotionEvent): Boolean {
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                val pointerIndex = event.actionIndex
                val x = event.getX(pointerIndex)
                val y = event.getY(pointerIndex)
                if (isInside(x, y)) {
                    centerX = x
                    centerY = y
                    currentPointerId = event.getPointerId(pointerIndex)
                    updateState(true, 0.0f, 0.0f)
                    return true
                }
            }

            MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP -> {
                if (currentPointerId == event.getPointerId(event.actionIndex)) {
                    centerX = originalCenterX
                    centerY = originalCenterY
                    currentPointerId = -1
                    updateState(false, 0.0f, 0.0f)
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
                    var x: Float = (event.getX(i) - centerX) / radius
                    var y: Float = (event.getY(i) - centerY) / radius
                    val norm = sqrt(x * x + y * y)
                    if (norm > 1.0f) {
                        x /= norm
                        y /= norm
                    }
                    updateState(true, x, y)
                    return true
                }
            }
        }
        return false
    }

    override fun resetInput() {
        updateState(false, 0f, 0f)
    }

    override fun drawInput(canvas: Canvas) {
        paint.color = backgroundFillColor
        paint.style = Paint.Style.FILL
        canvas.drawCircle(centerX, centerY, radius, paint)

        val fillColor: Int
        val strokeColor: Int
        if (pressed) {
            fillColor = activeFillColor
            strokeColor = activeStrokeColor
        } else {
            fillColor = inactiveFillColor
            strokeColor = inactiveStrokeColor
        }

        canvas.fillCircleWithStroke(
            innerCenterX,
            innerCenterY,
            innerRadius,
            paint,
            fillColor,
            strokeColor
        )
    }

    override fun isInside(x: Float, y: Float): Boolean {
        return (x - originalCenterX) * (x - originalCenterX) + (y - originalCenterY) * (y - originalCenterY) <= radius * radius
    }
}
