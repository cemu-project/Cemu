package info.cemu.Cemu.settings.input;

import info.cemu.Cemu.R;
import info.cemu.Cemu.NativeLibrary;

public class ControllerTypeResourceNameMapper {
    public static int controllerTypeToResourceNameId(int type) {
        return switch (type) {
            case NativeLibrary.EMULATED_CONTROLLER_TYPE_DISABLED -> R.string.disabled;
            case NativeLibrary.EMULATED_CONTROLLER_TYPE_VPAD -> R.string.vpad_controller;
            case NativeLibrary.EMULATED_CONTROLLER_TYPE_PRO -> R.string.pro_controller;
            case NativeLibrary.EMULATED_CONTROLLER_TYPE_WIIMOTE -> R.string.wiimote_controller;
            case NativeLibrary.EMULATED_CONTROLLER_TYPE_CLASSIC -> R.string.classic_controller;
            default -> throw new IllegalArgumentException("Invalid controller type: " + type);
        };
    }

}
