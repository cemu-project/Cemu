package info.cemu.cemu.emulation.inputoverlay

import android.graphics.Canvas
import android.graphics.Matrix
import android.graphics.Paint
import android.graphics.Path
import android.graphics.PointF
import android.graphics.Rect
import android.graphics.RectF
import kotlin.math.cos
import kotlin.math.min
import kotlin.math.sin

class LineBounds(
    var startX: Float = 0.0f,
    var startY: Float = 0.0f,
    var stopX: Float = 0.0f,
    var stopY: Float = 0.0f,
) {
    fun setFromPoints(start: PointF, stop: PointF) {
        this.startX = start.x
        this.startY = start.y
        this.stopX = stop.x
        this.stopY = stop.y
    }
}

fun Path.fitInsideRectangle(boundingRect: Rect, canvasSize: Float) {
    val transformMatrix = Matrix()
    val rectSize = min(boundingRect.width(), boundingRect.height()) * 0.85f
    val scale = rectSize / canvasSize
    transformMatrix.setScale(scale, scale)
    transformMatrix.postTranslate(
        boundingRect.exactCenterX() - rectSize * 0.5f,
        boundingRect.exactCenterY() + rectSize * 0.5f
    )

    transform(transformMatrix)
}

fun PointF.rotate(pivotX: Float, pivotY: Float, angleDegrees: Float) {
    val angleRad = Math.toRadians(angleDegrees.toDouble()).toFloat()

    val dx = x - pivotX
    val dy = y - pivotY

    val cosA = cos(angleRad)
    val sinA = sin(angleRad)
    val newX = dx * cosA - dy * sinA
    val newY = dx * sinA + dy * cosA

    x = newX + pivotX
    y = newY + pivotY
}

fun Canvas.drawLine(lineBounds: LineBounds, paint: Paint) {
    drawLine(lineBounds.startX, lineBounds.startY, lineBounds.stopX, lineBounds.stopY, paint)
}

fun Canvas.fillCircleWithStroke(
    centerX: Float,
    centerY: Float,
    radius: Float,
    paint: Paint,
    fillColor: Int,
    strokeColor: Int,
) {
    paint.color = fillColor
    paint.style = Paint.Style.FILL
    drawCircle(centerX, centerY, radius, paint)
    paint.color = strokeColor
    paint.style = Paint.Style.STROKE
    drawCircle(centerX, centerY, radius, paint)
}

fun Canvas.fillRoundRectangleWithStroke(
    left: Float,
    top: Float,
    right: Float,
    bottom: Float,
    radius: Float,
    paint: Paint,
    fillColor: Int,
    strokeColor: Int,
) {
    fillRoundRectangleWithStroke(
        RectF(left, top, right, bottom),
        radius,
        paint,
        fillColor,
        strokeColor,
    )
}

fun Canvas.fillRoundRectangleWithStroke(
    rect: RectF,
    radius: Float,
    paint: Paint,
    fillColor: Int,
    strokeColor: Int,
) {
    paint.style = Paint.Style.FILL
    paint.color = fillColor
    drawRoundRect(rect, radius, radius, paint)
    paint.style = Paint.Style.STROKE
    paint.color = strokeColor
    drawRoundRect(rect, radius, radius, paint)
}

fun Canvas.fillPathWithStroke(
    path: Path,
    paint: Paint,
    fillColor: Int,
    strokeColor: Int,
) {
    paint.style = Paint.Style.FILL
    paint.color = fillColor
    drawPath(path, paint)
    paint.style = Paint.Style.STROKE
    paint.color = strokeColor
    drawPath(path, paint)
}
