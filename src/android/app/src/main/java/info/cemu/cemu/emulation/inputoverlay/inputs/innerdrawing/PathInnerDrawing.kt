package info.cemu.cemu.emulation.inputoverlay.inputs.innerdrawing

import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Path
import android.graphics.Rect
import info.cemu.cemu.emulation.inputoverlay.Colors
import info.cemu.cemu.emulation.inputoverlay.fitInsideRectangle

abstract class PathInnerDrawing : ButtonInnerDrawing {
    private var activeColor = 0
    private var inactiveColor = 0
    private val paint = Paint()
    private var path = Path()
    override fun draw(canvas: Canvas, state: Boolean) {
        paint.color = if (state) activeColor else inactiveColor
        canvas.drawPath(path, paint)
    }

    protected abstract val canvasSize: Float
    protected val originalPath: Path by lazy(LazyThreadSafetyMode.NONE) { createOriginalPath() }

    protected abstract fun createOriginalPath(): Path

    override fun configure(boundingRect: Rect, alpha: Int) {
        activeColor = Colors.activeStroke(alpha)
        inactiveColor = Colors.inactiveStroke(alpha)

        path = Path(originalPath)

        path.fitInsideRectangle(boundingRect, canvasSize)
    }
}