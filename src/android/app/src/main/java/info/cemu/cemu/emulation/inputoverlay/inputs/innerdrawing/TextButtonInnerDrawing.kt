package info.cemu.cemu.emulation.inputoverlay.inputs.innerdrawing

import android.graphics.Canvas
import android.graphics.Rect
import android.text.StaticLayout
import android.text.TextPaint
import androidx.core.graphics.withTranslation
import info.cemu.cemu.emulation.inputoverlay.Colors
import kotlin.math.min

class TextButtonInnerDrawing(private val text: String) : ButtonInnerDrawing {
    private val textPaint = TextPaint()
    private var staticLayout = createStaticLayout()
    private var textXCoordinate = 0f
    private var textYCoordinate = 0f
    private var activeColor = 0
    private var inactiveColor = 0

    override fun draw(canvas: Canvas, state: Boolean) {
        textPaint.color = if (state) activeColor else inactiveColor
        canvas.withTranslation(textXCoordinate, textYCoordinate) {
            staticLayout.draw(canvas)
        }
    }

    override fun configure(boundingRect: Rect, alpha: Int) {
        textPaint.textSize = min(boundingRect.width(), boundingRect.height()) * 0.65f
        activeColor = Colors.activeStroke(alpha)
        inactiveColor = Colors.inactiveStroke(alpha)
        staticLayout = createStaticLayout()
        textXCoordinate = boundingRect.exactCenterX() - staticLayout.width * 0.5f
        textYCoordinate = boundingRect.exactCenterY() - staticLayout.height * 0.5f
    }

    private fun createStaticLayout(): StaticLayout {
        val textWidth = textPaint.measureText(text).toInt()
        return StaticLayout.Builder.obtain(text, 0, text.length, textPaint, textWidth)
            .setIncludePad(false)
            .build()
    }
}