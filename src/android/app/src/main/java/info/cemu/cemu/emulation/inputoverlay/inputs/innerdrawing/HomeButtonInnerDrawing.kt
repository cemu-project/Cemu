package info.cemu.cemu.emulation.inputoverlay.inputs.innerdrawing

import android.graphics.Path

class HomeButtonInnerDrawing : PathInnerDrawing() {
    override val canvasSize: Float = 960f

    override fun createOriginalPath(): Path = Path().apply {
        moveTo(240f, -200f)
        rLineTo(120f, 0f)
        rLineTo(0f, -240f)
        rLineTo(240f, 0f)
        rLineTo(0f, 240f)
        rLineTo(120f, 0f)
        rLineTo(0f, -360f)
        lineTo(480f, -740f)
        lineTo(240f, -560f)
        rLineTo(0f, 360f)
        close()

        rMoveTo(-80f, 80f)
        rLineTo(0f, -480f)
        rLineTo(320f, -240f)
        rLineTo(320f, 240f)
        rLineTo(0f, 480f)
        lineTo(520f, -120f)
        rLineTo(0f, -240f)
        rLineTo(-80f, 0f)
        rLineTo(0f, 240f)
        lineTo(160f, -120f)
        close()

        rMoveTo(320f, -350f)
        close()
    }
}