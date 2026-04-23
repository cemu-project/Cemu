package info.cemu.cemu.emulation.inputoverlay.inputs

import android.graphics.Canvas
import android.graphics.Rect
import info.cemu.cemu.emulation.inputoverlay.fillCircleWithStroke
import info.cemu.cemu.emulation.inputoverlay.inputs.innerdrawing.ButtonInnerDrawing
import kotlin.math.min

class RoundButton(
    private val innerDrawing: ButtonInnerDrawing,
    onButtonStateChange: (state: Boolean) -> Unit,
    private val alpha: Int,
    rect: Rect,
) : Button(onButtonStateChange, rect) {
    private var centerX: Float = 0f
    private var centerY: Float = 0f
    private var radius: Float = 0f
    private var radius2: Float = 0f

    init {
        configure()
    }

    override fun configure() {
        centerX = rect.centerX().toFloat()
        centerY = rect.centerY().toFloat()
        radius = min(rect.width().toFloat(), rect.height().toFloat()) / 2f
        radius2 = radius * radius
        innerDrawing.configure(rect, alpha)
        configureColors(alpha)
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
        canvas.fillCircleWithStroke(centerX, centerY, radius, paint, fillColor, strokeColor)
        innerDrawing.draw(canvas, state)
    }

    override fun isInside(x: Float, y: Float): Boolean {
        val dx = x - centerX
        val dy = y - centerY
        return dx * dx + dy * dy <= radius2
    }
}
