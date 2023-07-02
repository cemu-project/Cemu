package info.cemu.Cemu;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

public class ControllerInputsDataProvider {
    private int getButtonResourceIdName(int controllerType, int buttonId) {
        if (controllerType == NativeLibrary.EMULATED_CONTROLLER_TYPE_VPAD) {
            return switch (buttonId) {
                case NativeLibrary.VPAD_BUTTON_A -> R.string.button_a;
                case NativeLibrary.VPAD_BUTTON_B -> R.string.button_b;
                case NativeLibrary.VPAD_BUTTON_X -> R.string.button_x;
                case NativeLibrary.VPAD_BUTTON_Y -> R.string.button_y;
                case NativeLibrary.VPAD_BUTTON_L -> R.string.button_l;
                case NativeLibrary.VPAD_BUTTON_R -> R.string.button_r;
                case NativeLibrary.VPAD_BUTTON_ZL -> R.string.button_zl;
                case NativeLibrary.VPAD_BUTTON_ZR -> R.string.button_zr;
                case NativeLibrary.VPAD_BUTTON_PLUS -> R.string.button_plus;
                case NativeLibrary.VPAD_BUTTON_MINUS -> R.string.button_minus;
                case NativeLibrary.VPAD_BUTTON_UP -> R.string.button_up;
                case NativeLibrary.VPAD_BUTTON_DOWN -> R.string.button_down;
                case NativeLibrary.VPAD_BUTTON_LEFT -> R.string.button_left;
                case NativeLibrary.VPAD_BUTTON_RIGHT -> R.string.button_right;
                case NativeLibrary.VPAD_BUTTON_STICKL -> R.string.button_stickl;
                case NativeLibrary.VPAD_BUTTON_STICKR -> R.string.button_stickr;
                case NativeLibrary.VPAD_BUTTON_STICKL_UP -> R.string.button_stickl_up;
                case NativeLibrary.VPAD_BUTTON_STICKL_DOWN -> R.string.button_stickl_down;
                case NativeLibrary.VPAD_BUTTON_STICKL_LEFT -> R.string.button_stickl_left;
                case NativeLibrary.VPAD_BUTTON_STICKL_RIGHT -> R.string.button_stickl_right;
                case NativeLibrary.VPAD_BUTTON_STICKR_UP -> R.string.button_stickr_up;
                case NativeLibrary.VPAD_BUTTON_STICKR_DOWN -> R.string.button_stickr_down;
                case NativeLibrary.VPAD_BUTTON_STICKR_LEFT -> R.string.button_stickr_left;
                case NativeLibrary.VPAD_BUTTON_STICKR_RIGHT -> R.string.button_stickr_right;
                case NativeLibrary.VPAD_BUTTON_MIC -> R.string.button_mic;
                case NativeLibrary.VPAD_BUTTON_SCREEN -> R.string.button_screen;
                case NativeLibrary.VPAD_BUTTON_HOME -> R.string.button_home;
                default ->
                        throw new RuntimeException("Invalid buttonId " + buttonId + " for controllerType " + controllerType);
            };
        }
        if (controllerType == NativeLibrary.EMULATED_CONTROLLER_TYPE_PRO) {
            return switch (buttonId) {
                case NativeLibrary.PRO_BUTTON_A -> R.string.button_a;
                case NativeLibrary.PRO_BUTTON_B -> R.string.button_b;
                case NativeLibrary.PRO_BUTTON_X -> R.string.button_x;
                case NativeLibrary.PRO_BUTTON_Y -> R.string.button_y;
                case NativeLibrary.PRO_BUTTON_L -> R.string.button_l;
                case NativeLibrary.PRO_BUTTON_R -> R.string.button_r;
                case NativeLibrary.PRO_BUTTON_ZL -> R.string.button_zl;
                case NativeLibrary.PRO_BUTTON_ZR -> R.string.button_zr;
                case NativeLibrary.PRO_BUTTON_PLUS -> R.string.button_plus;
                case NativeLibrary.PRO_BUTTON_MINUS -> R.string.button_minus;
                case NativeLibrary.PRO_BUTTON_HOME -> R.string.button_home;
                case NativeLibrary.PRO_BUTTON_UP -> R.string.button_up;
                case NativeLibrary.PRO_BUTTON_DOWN -> R.string.button_down;
                case NativeLibrary.PRO_BUTTON_LEFT -> R.string.button_left;
                case NativeLibrary.PRO_BUTTON_RIGHT -> R.string.button_right;
                case NativeLibrary.PRO_BUTTON_STICKL -> R.string.button_stickl;
                case NativeLibrary.PRO_BUTTON_STICKR -> R.string.button_stickr;
                case NativeLibrary.PRO_BUTTON_STICKL_UP -> R.string.button_stickl_up;
                case NativeLibrary.PRO_BUTTON_STICKL_DOWN -> R.string.button_stickl_down;
                case NativeLibrary.PRO_BUTTON_STICKL_LEFT -> R.string.button_stickl_left;
                case NativeLibrary.PRO_BUTTON_STICKL_RIGHT -> R.string.button_stickl_right;
                case NativeLibrary.PRO_BUTTON_STICKR_UP -> R.string.button_stickr_up;
                case NativeLibrary.PRO_BUTTON_STICKR_DOWN -> R.string.button_stickr_down;
                case NativeLibrary.PRO_BUTTON_STICKR_LEFT -> R.string.button_stickr_left;
                case NativeLibrary.PRO_BUTTON_STICKR_RIGHT -> R.string.button_stickr_right;
                default ->
                        throw new RuntimeException("Invalid buttonId " + buttonId + " for controllerType " + controllerType);
            };
        }
        if (controllerType == NativeLibrary.EMULATED_CONTROLLER_TYPE_CLASSIC) {
            return switch (buttonId) {
                case NativeLibrary.CLASSIC_BUTTON_A -> R.string.button_a;
                case NativeLibrary.CLASSIC_BUTTON_B -> R.string.button_b;
                case NativeLibrary.CLASSIC_BUTTON_X -> R.string.button_x;
                case NativeLibrary.CLASSIC_BUTTON_Y -> R.string.button_y;
                case NativeLibrary.CLASSIC_BUTTON_L -> R.string.button_l;
                case NativeLibrary.CLASSIC_BUTTON_R -> R.string.button_r;
                case NativeLibrary.CLASSIC_BUTTON_ZL -> R.string.button_zl;
                case NativeLibrary.CLASSIC_BUTTON_ZR -> R.string.button_zr;
                case NativeLibrary.CLASSIC_BUTTON_PLUS -> R.string.button_plus;
                case NativeLibrary.CLASSIC_BUTTON_MINUS -> R.string.button_minus;
                case NativeLibrary.CLASSIC_BUTTON_HOME -> R.string.button_home;
                case NativeLibrary.CLASSIC_BUTTON_UP -> R.string.button_up;
                case NativeLibrary.CLASSIC_BUTTON_DOWN -> R.string.button_down;
                case NativeLibrary.CLASSIC_BUTTON_LEFT -> R.string.button_left;
                case NativeLibrary.CLASSIC_BUTTON_RIGHT -> R.string.button_right;
                case NativeLibrary.CLASSIC_BUTTON_STICKL_UP -> R.string.button_stickl_up;
                case NativeLibrary.CLASSIC_BUTTON_STICKL_DOWN -> R.string.button_stickl_down;
                case NativeLibrary.CLASSIC_BUTTON_STICKL_LEFT -> R.string.button_stickl_left;
                case NativeLibrary.CLASSIC_BUTTON_STICKL_RIGHT -> R.string.button_stickl_right;
                case NativeLibrary.CLASSIC_BUTTON_STICKR_UP -> R.string.button_stickr_up;
                case NativeLibrary.CLASSIC_BUTTON_STICKR_DOWN -> R.string.button_stickr_down;
                case NativeLibrary.CLASSIC_BUTTON_STICKR_LEFT -> R.string.button_stickr_left;
                case NativeLibrary.CLASSIC_BUTTON_STICKR_RIGHT -> R.string.button_stickr_right;
                default ->
                        throw new RuntimeException("Invalid buttonId " + buttonId + " for controllerType " + controllerType);
            };
        }
        if (controllerType == NativeLibrary.EMULATED_CONTROLLER_TYPE_WIIMOTE) {
            return switch (buttonId) {
                case NativeLibrary.WIIMOTE_BUTTON_A -> R.string.button_a;
                case NativeLibrary.WIIMOTE_BUTTON_B -> R.string.button_b;
                case NativeLibrary.WIIMOTE_BUTTON_1 -> R.string.button_1;
                case NativeLibrary.WIIMOTE_BUTTON_2 -> R.string.button_2;
                case NativeLibrary.WIIMOTE_BUTTON_NUNCHUCK_Z -> R.string.button_nunchuck_z;
                case NativeLibrary.WIIMOTE_BUTTON_NUNCHUCK_C -> R.string.button_nunchuck_c;
                case NativeLibrary.WIIMOTE_BUTTON_PLUS -> R.string.button_plus;
                case NativeLibrary.WIIMOTE_BUTTON_MINUS -> R.string.button_minus;
                case NativeLibrary.WIIMOTE_BUTTON_UP -> R.string.button_up;
                case NativeLibrary.WIIMOTE_BUTTON_DOWN -> R.string.button_down;
                case NativeLibrary.WIIMOTE_BUTTON_LEFT -> R.string.button_left;
                case NativeLibrary.WIIMOTE_BUTTON_RIGHT -> R.string.button_right;
                case NativeLibrary.WIIMOTE_BUTTON_NUNCHUCK_UP -> R.string.button_nunchuck_up;
                case NativeLibrary.WIIMOTE_BUTTON_NUNCHUCK_DOWN -> R.string.button_nunchuck_down;
                case NativeLibrary.WIIMOTE_BUTTON_NUNCHUCK_LEFT -> R.string.button_nunchuck_left;
                case NativeLibrary.WIIMOTE_BUTTON_NUNCHUCK_RIGHT -> R.string.button_nunchuck_right;
                case NativeLibrary.WIIMOTE_BUTTON_HOME -> R.string.button_home;
                default ->
                        throw new RuntimeException("Invalid buttonId " + buttonId + " for controllerType " + controllerType);
            };
        }
        throw new RuntimeException("Invalid controllerType " + controllerType);
    }

    private void addControllerInputsGroup(List<ControllerItem> controllerInputItems, String groupName, int controllerType, int[] buttons, Map<Integer, String> inputs) {
        controllerInputItems.add(new ControllerHeaderItem(groupName));
        for (int button : buttons) {
            controllerInputItems.add(new ControllerInputItem(getButtonResourceIdName(controllerType, button), button, inputs.getOrDefault(button, "")));
        }
    }

    public List<ControllerItem> getControllerInputs(int controllerType, Map<Integer, String> inputs) {
        List<ControllerItem> controllerInputItems = new ArrayList<>();
        if (controllerType == NativeLibrary.EMULATED_CONTROLLER_TYPE_VPAD) {
            addControllerInputsGroup(controllerInputItems, "Buttons", controllerType, new int[]{NativeLibrary.VPAD_BUTTON_A, NativeLibrary.VPAD_BUTTON_B, NativeLibrary.VPAD_BUTTON_X, NativeLibrary.VPAD_BUTTON_Y, NativeLibrary.VPAD_BUTTON_L, NativeLibrary.VPAD_BUTTON_R, NativeLibrary.VPAD_BUTTON_ZL, NativeLibrary.VPAD_BUTTON_ZR, NativeLibrary.VPAD_BUTTON_PLUS, NativeLibrary.VPAD_BUTTON_MINUS}, inputs);
            addControllerInputsGroup(controllerInputItems, "D-Pad", controllerType, new int[]{NativeLibrary.VPAD_BUTTON_UP, NativeLibrary.VPAD_BUTTON_DOWN, NativeLibrary.VPAD_BUTTON_LEFT, NativeLibrary.VPAD_BUTTON_RIGHT}, inputs);
            addControllerInputsGroup(controllerInputItems, "Left Axis", controllerType, new int[]{NativeLibrary.VPAD_BUTTON_STICKL, NativeLibrary.VPAD_BUTTON_STICKL_UP, NativeLibrary.VPAD_BUTTON_STICKL_DOWN, NativeLibrary.VPAD_BUTTON_STICKL_LEFT, NativeLibrary.VPAD_BUTTON_STICKL_RIGHT}, inputs);
            addControllerInputsGroup(controllerInputItems, "Right Axis", controllerType, new int[]{NativeLibrary.VPAD_BUTTON_STICKR, NativeLibrary.VPAD_BUTTON_STICKR_UP, NativeLibrary.VPAD_BUTTON_STICKR_DOWN, NativeLibrary.VPAD_BUTTON_STICKR_LEFT, NativeLibrary.VPAD_BUTTON_STICKR_RIGHT}, inputs);
            addControllerInputsGroup(controllerInputItems, "Extra", controllerType, new int[]{NativeLibrary.VPAD_BUTTON_MIC, NativeLibrary.VPAD_BUTTON_SCREEN, NativeLibrary.VPAD_BUTTON_HOME}, inputs);
        }
        if (controllerType == NativeLibrary.EMULATED_CONTROLLER_TYPE_PRO) {
            addControllerInputsGroup(controllerInputItems, "Buttons", controllerType, new int[]{NativeLibrary.PRO_BUTTON_A, NativeLibrary.PRO_BUTTON_B, NativeLibrary.PRO_BUTTON_X, NativeLibrary.PRO_BUTTON_Y, NativeLibrary.PRO_BUTTON_L, NativeLibrary.PRO_BUTTON_R, NativeLibrary.PRO_BUTTON_ZL, NativeLibrary.PRO_BUTTON_ZR, NativeLibrary.PRO_BUTTON_PLUS, NativeLibrary.PRO_BUTTON_MINUS, NativeLibrary.PRO_BUTTON_HOME}, inputs);
            addControllerInputsGroup(controllerInputItems, "Left Axis", controllerType, new int[]{NativeLibrary.PRO_BUTTON_STICKL, NativeLibrary.PRO_BUTTON_STICKL_UP, NativeLibrary.PRO_BUTTON_STICKL_DOWN, NativeLibrary.PRO_BUTTON_STICKL_LEFT, NativeLibrary.PRO_BUTTON_STICKL_RIGHT}, inputs);
            addControllerInputsGroup(controllerInputItems, "Right Axis", controllerType, new int[]{NativeLibrary.PRO_BUTTON_STICKR, NativeLibrary.PRO_BUTTON_STICKR_UP, NativeLibrary.PRO_BUTTON_STICKR_DOWN, NativeLibrary.PRO_BUTTON_STICKR_LEFT, NativeLibrary.PRO_BUTTON_STICKR_RIGHT}, inputs);
            addControllerInputsGroup(controllerInputItems, "D-Pad", controllerType, new int[]{NativeLibrary.PRO_BUTTON_UP, NativeLibrary.PRO_BUTTON_DOWN, NativeLibrary.PRO_BUTTON_LEFT, NativeLibrary.PRO_BUTTON_RIGHT}, inputs);
        }
        if (controllerType == NativeLibrary.EMULATED_CONTROLLER_TYPE_CLASSIC) {
            addControllerInputsGroup(controllerInputItems, "Buttons", controllerType, new int[]{NativeLibrary.CLASSIC_BUTTON_A, NativeLibrary.CLASSIC_BUTTON_B, NativeLibrary.CLASSIC_BUTTON_X, NativeLibrary.CLASSIC_BUTTON_Y, NativeLibrary.CLASSIC_BUTTON_L, NativeLibrary.CLASSIC_BUTTON_R, NativeLibrary.CLASSIC_BUTTON_ZL, NativeLibrary.CLASSIC_BUTTON_ZR, NativeLibrary.CLASSIC_BUTTON_PLUS, NativeLibrary.CLASSIC_BUTTON_MINUS, NativeLibrary.CLASSIC_BUTTON_HOME}, inputs);
            addControllerInputsGroup(controllerInputItems, "Left Axis", controllerType, new int[]{NativeLibrary.CLASSIC_BUTTON_STICKL_UP, NativeLibrary.CLASSIC_BUTTON_STICKL_DOWN, NativeLibrary.CLASSIC_BUTTON_STICKL_LEFT, NativeLibrary.CLASSIC_BUTTON_STICKL_RIGHT}, inputs);
            addControllerInputsGroup(controllerInputItems, "Right Axis", controllerType, new int[]{NativeLibrary.CLASSIC_BUTTON_STICKR_UP, NativeLibrary.CLASSIC_BUTTON_STICKR_DOWN, NativeLibrary.CLASSIC_BUTTON_STICKR_LEFT, NativeLibrary.CLASSIC_BUTTON_STICKR_RIGHT}, inputs);
            addControllerInputsGroup(controllerInputItems, "D-Pad", controllerType, new int[]{NativeLibrary.CLASSIC_BUTTON_UP, NativeLibrary.CLASSIC_BUTTON_DOWN, NativeLibrary.CLASSIC_BUTTON_LEFT, NativeLibrary.CLASSIC_BUTTON_RIGHT}, inputs);
        }
        if (controllerType == NativeLibrary.EMULATED_CONTROLLER_TYPE_WIIMOTE) {
            addControllerInputsGroup(controllerInputItems, "Buttons", controllerType, new int[]{NativeLibrary.WIIMOTE_BUTTON_A, NativeLibrary.WIIMOTE_BUTTON_B, NativeLibrary.WIIMOTE_BUTTON_1, NativeLibrary.WIIMOTE_BUTTON_2, NativeLibrary.WIIMOTE_BUTTON_NUNCHUCK_Z, NativeLibrary.WIIMOTE_BUTTON_NUNCHUCK_C, NativeLibrary.WIIMOTE_BUTTON_PLUS, NativeLibrary.WIIMOTE_BUTTON_MINUS, NativeLibrary.WIIMOTE_BUTTON_HOME}, inputs);
            addControllerInputsGroup(controllerInputItems, "D-Pad", controllerType, new int[]{NativeLibrary.WIIMOTE_BUTTON_UP, NativeLibrary.WIIMOTE_BUTTON_DOWN, NativeLibrary.WIIMOTE_BUTTON_LEFT, NativeLibrary.WIIMOTE_BUTTON_RIGHT}, inputs);
            addControllerInputsGroup(controllerInputItems, "Nunchuck", controllerType, new int[]{NativeLibrary.WIIMOTE_BUTTON_NUNCHUCK_UP, NativeLibrary.WIIMOTE_BUTTON_NUNCHUCK_DOWN, NativeLibrary.WIIMOTE_BUTTON_NUNCHUCK_LEFT, NativeLibrary.WIIMOTE_BUTTON_NUNCHUCK_RIGHT}, inputs);
        }
        return controllerInputItems;
    }

}
