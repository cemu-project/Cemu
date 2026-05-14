package info.cemu.cemu.emulation

import android.annotation.SuppressLint
import android.view.MotionEvent
import android.view.View
import info.cemu.cemu.nativeinterface.NativeInput

class CanvasOnTouchListener(val isTV: Boolean) : View.OnTouchListener {
    private var currentPointerId: Int = -1

    @SuppressLint("ClickableViewAccessibility")
    override fun onTouch(v: View, event: MotionEvent): Boolean {
        val pointerIndex = event.actionIndex
        val pointerId = event.getPointerId(pointerIndex)
        if (currentPointerId != -1 && pointerId != currentPointerId) {
            return false
        }
        val x = event.getX(pointerIndex).toInt()
        val y = event.getY(pointerIndex).toInt()
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                NativeInput.onTouchDown(x, y, isTV)
                return true
            }

            MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP -> {
                currentPointerId = -1
                NativeInput.onTouchUp(x, y, isTV)
                return true
            }

            MotionEvent.ACTION_MOVE -> {
                NativeInput.onTouchMove(x, y, isTV)
                return true
            }
        }
        return false
    }
}