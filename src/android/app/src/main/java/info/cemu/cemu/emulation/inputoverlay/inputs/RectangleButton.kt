package info.cemu.cemu.emulation.inputoverlay.inputs

import android.graphics.Canvas
import android.graphics.Rect
import info.cemu.cemu.emulation.inputoverlay.fillRoundRectangleWithStroke
import info.cemu.cemu.emulation.inputoverlay.inputs.innerdrawing.ButtonInnerDrawing

class RectangleButton(
    private val innerDrawing: ButtonInnerDrawing,
    onButtonStateChange: (state: Boolean) -> Unit,
    private val alpha: Int,
    rect: Rect,
) : Button(onButtonStateChange, rect) {
    var left: Float = 0f
    var top: Float = 0f
    var right: Float = 0f
    var bottom: Float = 0f

    init {
        configure()
    }

    override fun configure() {
        this.left = rect.left.toFloat()
        this.top = rect.top.toFloat()
        this.right = rect.right.toFloat()
        this.bottom = rect.bottom.toFloat()
        configureColors(alpha)
        innerDrawing.configure(rect, alpha)
    }

    override fun drawInput(canvas: Canvas) {
        val fillColor: Int
        val strokeColor: Int
        if (state) {
            fillColor = activeFillColor
            strokeColor = activeStrokeColor
        } else {
            fillColor = inactiveFillColor
            strokeColor = inactiveStrokeColor
        }
        canvas.fillRoundRectangleWithStroke(
            left,
            top,
            right,
            bottom,
            RECTANGLE_RADIUS,
            paint,
            fillColor,
            strokeColor
        )
        innerDrawing.draw(canvas, state)
    }

    override fun isInside(x: Float, y: Float): Boolean {
        return x in left..<right && y in top..<bottom
    }

    companion object {
        const val RECTANGLE_RADIUS = 5f
    }
}
