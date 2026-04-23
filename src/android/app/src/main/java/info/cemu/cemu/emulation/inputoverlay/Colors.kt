package info.cemu.cemu.emulation.inputoverlay

import android.graphics.Color

object Colors {
    fun activeFill(alpha: Int) = Color.argb(alpha, 255, 255, 255)
    fun activeStroke(alpha: Int) = Color.argb(alpha, 0, 0, 0)

    fun inactiveFill(alpha: Int) = Color.argb(alpha, 0, 0, 0)
    fun inactiveStroke(alpha: Int) = Color.argb(alpha, 255, 255, 255)

    fun backgroundFill(alpha: Int) = Color.argb(alpha, 128, 128, 128)
    fun backgroundStroke(alpha: Int) = Color.argb(alpha, 200, 200, 200)
}
