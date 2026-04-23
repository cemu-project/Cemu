package info.cemu.cemu.emulation.inputoverlay.inputs

import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Rect
import android.view.MotionEvent
import androidx.annotation.ColorInt
import info.cemu.cemu.common.settings.InputOverlayRect
import info.cemu.cemu.emulation.inputoverlay.Colors
import kotlin.math.max
import kotlin.math.min

abstract class Input protected constructor(
    protected var rect: Rect,
) {
    private var drawBoundingRectangle = false
    private val boundingRectanglePaint = Paint()
    protected var activeFillColor = 0
    protected var activeStrokeColor = 0
    protected var inactiveFillColor = 0
    protected var inactiveStrokeColor = 0
    protected val paint = Paint().apply {
        strokeWidth = 3f
    }

    fun getBoundingRectangle() = InputOverlayRect(
        left = rect.left,
        top = rect.top,
        right = rect.right,
        bottom = rect.bottom,
    )

    abstract fun onTouch(event: MotionEvent): Boolean

    fun moveInput(x: Int, y: Int, maxWidth: Int, maxHeight: Int) {
        val width = rect.width()
        val height = rect.height()
        val left = min(
            max(x - width / 2, 0),
            maxWidth - width
        )
        val top = min(
            max(y - height / 2, 0),
            maxHeight - height
        )
        rect = Rect(
            left,
            top,
            left + width,
            top + height
        )
        configure()
    }

    fun resize(diffX: Int, diffY: Int, maxWidth: Int, maxHeight: Int, minWidthHeight: Int) {
        val newRight = rect.right + diffX
        val newBottom = rect.bottom + diffY
        if (newRight - rect.left < minWidthHeight
            || newBottom - rect.top < minWidthHeight
            || newRight > maxWidth
            || newBottom > maxHeight
        ) {
            return
        }
        rect = Rect(
            rect.left,
            rect.top,
            newRight,
            newBottom
        )
        configure()
    }

    protected abstract fun resetInput()

    fun reset() {
        drawBoundingRectangle = false
    }

    protected abstract fun configure()

    protected fun configureColors(alpha: Int) {
        activeFillColor = Colors.activeFill(alpha)
        activeStrokeColor = Colors.activeStroke(alpha)
        inactiveFillColor = Colors.inactiveFill(alpha)
        inactiveStrokeColor = Colors.inactiveStroke(alpha)
    }

    protected abstract fun drawInput(canvas: Canvas)

    fun draw(canvas: Canvas) {
        if (drawBoundingRectangle) {
            canvas.drawRect(rect, boundingRectanglePaint)
        }
        drawInput(canvas)
    }

    fun enableDrawingBoundingRect(@ColorInt color: Int) {
        drawBoundingRectangle = true
        boundingRectanglePaint.color = color
    }

    fun disableDrawingBoundingRect() {
        drawBoundingRectangle = false
    }

    abstract fun isInside(x: Float, y: Float): Boolean
}
