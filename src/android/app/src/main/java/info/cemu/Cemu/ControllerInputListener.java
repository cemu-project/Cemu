package info.cemu.Cemu;

import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;

public class ControllerInputListener {
    private static boolean isController(int sources) {
        return ((sources & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK) ||
                ((sources & InputDevice.SOURCE_DPAD) == InputDevice.SOURCE_DPAD) ||
                ((sources & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD);
    }

    public static boolean onKeyEvent(KeyEvent keyEvent) {
        InputDevice inputDevice = keyEvent.getDevice();
        if (isController(inputDevice.getSources())) {
            String descriptor = inputDevice.getDescriptor();
            String name = inputDevice.getName();
            NativeLibrary.onNativeKey(descriptor == null ? name : descriptor, name, keyEvent.getKeyCode(), keyEvent.getAction() == KeyEvent.ACTION_DOWN);
            return true;
        }
        return false;
    }

    public static boolean onMotionEvent(MotionEvent motionEvent) {
        int actionPointerIndex = motionEvent.getActionIndex();
        int action = motionEvent.getActionMasked();
        if (action == MotionEvent.ACTION_MOVE) {
            InputDevice inputDevice = motionEvent.getDevice();
            String descriptor = inputDevice.getDescriptor();
            String name = inputDevice.getName();
            for (InputDevice.MotionRange range : inputDevice.getMotionRanges()) {
                int axis = range.getAxis();
                float value = (motionEvent.getAxisValue(axis, actionPointerIndex) - range.getMin()) / range.getRange() * 2.0f - 1.0f;
                NativeLibrary.onNativeAxis(descriptor == null ? name : descriptor, name, axis, value);
            }
            return true;
        }
        return false;
    }
}
