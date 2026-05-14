package info.cemu.cemu.emulation.inputoverlay.inputs.innerdrawing

import android.graphics.Canvas
import android.graphics.Rect

interface ButtonInnerDrawing {
    fun draw(canvas: Canvas, state: Boolean)
    fun configure(boundingRect: Rect, alpha: Int)
}