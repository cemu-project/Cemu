package info.cemu.Cemu.inputoverlay;


import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Rect;

import java.util.Optional;
import java.util.function.Consumer;

import info.cemu.Cemu.NativeLibrary;

public class InputOverlaySettingsProvider {
    public enum Input {
        A,
        B,
        ONE,
        TWO,
        C,
        Z,
        HOME,
        DPAD,
        L,
        L_STICK,
        MINUS,
        PLUS,
        R,
        R_STICK,
        X,
        Y,
        ZL,
        ZR,
        NUN_CHUCK_AXIS,
        LEFT_AXIS,
        RIGHT_AXIS,
    }

    private final SharedPreferences sharedPreferences;

    private static final int DEFAULT_ALPHA = 64;

    public InputOverlaySettingsProvider(Context context) {
        this.sharedPreferences = context.getSharedPreferences("settings", Context.MODE_PRIVATE);
    }

    public OverlaySettings getOverlaySettings() {
        return new OverlaySettings(
                isVibrateOnTouchEnabled(),
                isOverlayEnabled(),
                getControllerIndex(),
                getAlpha(),
                settings -> {
                    var editor = sharedPreferences.edit();
                    editor.putInt("controllerIndex", settings.getControllerIndex());
                    editor.putInt("alpha", settings.getAlpha());
                    editor.putBoolean("overlayEnabled", settings.isOverlayEnabled());
                    editor.putBoolean("vibrateOnTouchEnabled", settings.isVibrateOnTouchEnabled());
                    editor.apply();
                }
        );
    }

    public InputOverlaySettings getOverlaySettingsForInput(Input input, int width, int height) {
        return new InputOverlaySettings(
                getRectangle(input).orElseGet(() -> getDefaultRectangle(input, width, height)),
                getAlpha(),
                settings -> {
                    var editor = sharedPreferences.edit();
                    var rect = settings.getRect();
                    editor.putInt(input + "left", rect.left);
                    editor.putInt(input + "top", rect.top);
                    editor.putInt(input + "right", rect.right);
                    editor.putInt(input + "bottom", rect.bottom);
                    editor.putInt(input + "alpha", settings.getAlpha());
                    editor.apply();
                }
        );
    }

    private boolean isOverlayEnabled() {
        return sharedPreferences.getBoolean("overlayEnabled", true);
    }

    private boolean isVibrateOnTouchEnabled() {
        return sharedPreferences.getBoolean("vibrateOnTouchEnabled", true);
    }

    private int getControllerIndex() {
        return sharedPreferences.getInt("controllerIndex", 0);
    }

    private int getAlpha() {
        return sharedPreferences.getInt("alpha", DEFAULT_ALPHA);
    }

    private Optional<Rect> getRectangle(Input input) {
        int left = sharedPreferences.getInt(input + "left", -1);
        int top = sharedPreferences.getInt(input + "top", -1);
        int right = sharedPreferences.getInt(input + "right", -1);
        int bottom = sharedPreferences.getInt(input + "bottom", -1);
        if (left == -1 || top == -1 || right == -1 || bottom == -1) {
            return Optional.empty();
        }
        return Optional.of(new Rect(left, top, right, bottom));
    }

    public Rect getDefaultRectangle(Input input, int width, int height) {
        int pmButtonRadius = (int) (height * 0.065f);
        int pmButtonsCentreX = (int) (width * 0.5f);
        int pmButtonsCentreY = (int) (height * 0.875f);

        int roundButtonRadius = (int) (height * 0.065f);
        int abxyButtonsCentreX = width - roundButtonRadius * 4;
        int abxyButtonsCentreY = height - roundButtonRadius * 4;

        int rectangleButtonsHeight = (int) (height * 0.10f);
        int rectangleButtonsWidth = rectangleButtonsHeight * 2;

        int triggersLLeft = (int) (width * 0.05f);
        int triggersRLeft = (int) (width * 0.95f - rectangleButtonsWidth);
        int triggersTop = (int) (height * 0.05f);

        int dpadRadius = (int) (height * 0.20f);
        int dpadCentreX = dpadRadius;
        int dpadCentreY = (int) (height * 0.75f);

        int joystickRadius = (int) (height * 0.10f);
        int rightJoystickCentreX = (int) (width * 0.75f);
        int leftJoystickCentreX = (int) (width * 0.25f);
        int joystickCentreY = (int) (height * 0.55f);
        int joystickClickRadius = (int) (joystickRadius * 0.7f);

        return switch (input) {
            case A -> new Rect(abxyButtonsCentreX + roundButtonRadius,
                    abxyButtonsCentreY - roundButtonRadius,
                    abxyButtonsCentreX + roundButtonRadius * 3,
                    abxyButtonsCentreY + roundButtonRadius);
            case B -> new Rect(abxyButtonsCentreX - roundButtonRadius,
                    abxyButtonsCentreY + roundButtonRadius,
                    abxyButtonsCentreX + roundButtonRadius,
                    abxyButtonsCentreY + roundButtonRadius * 3);
            case X, ONE -> new Rect(abxyButtonsCentreX - roundButtonRadius,
                    abxyButtonsCentreY - roundButtonRadius * 3,
                    abxyButtonsCentreX + roundButtonRadius,
                    abxyButtonsCentreY - roundButtonRadius);
            case Y, TWO -> new Rect(abxyButtonsCentreX - roundButtonRadius * 3,
                    abxyButtonsCentreY - roundButtonRadius,
                    abxyButtonsCentreX - roundButtonRadius,
                    abxyButtonsCentreY + roundButtonRadius);
            case MINUS -> new Rect(pmButtonsCentreX - pmButtonRadius * 3,
                    pmButtonsCentreY - pmButtonRadius,
                    pmButtonsCentreX - pmButtonRadius,
                    pmButtonsCentreY + pmButtonRadius);
            case PLUS -> new Rect(pmButtonsCentreX + pmButtonRadius,
                    pmButtonsCentreY - pmButtonRadius,
                    pmButtonsCentreX + pmButtonRadius * 3,
                    pmButtonsCentreY + pmButtonRadius);
            case L -> new Rect(triggersLLeft,
                    triggersTop,
                    triggersLLeft + rectangleButtonsWidth,
                    triggersTop + rectangleButtonsHeight);
            case ZL -> new Rect(triggersLLeft,
                    (int) (triggersTop + rectangleButtonsHeight * 1.5f),
                    triggersLLeft + rectangleButtonsWidth,
                    (int) (triggersTop + rectangleButtonsHeight * 2.5f));
            case R -> new Rect(triggersRLeft,
                    triggersTop,
                    triggersRLeft + rectangleButtonsWidth,
                    triggersTop + rectangleButtonsHeight);
            case ZR -> new Rect(triggersRLeft,
                    (int) (triggersTop + rectangleButtonsHeight * 1.5f),
                    triggersRLeft + rectangleButtonsWidth,
                    (int) (triggersTop + rectangleButtonsHeight * 2.5f));
            // TODO: Wiimote settings
            case C, HOME, Z, NUN_CHUCK_AXIS -> new Rect();
            case DPAD -> new Rect(dpadCentreX - dpadRadius,
                    dpadCentreY - dpadRadius,
                    dpadCentreX + dpadRadius,
                    dpadCentreY + dpadRadius);
            case LEFT_AXIS -> new Rect(leftJoystickCentreX - joystickRadius,
                    joystickCentreY - joystickRadius,
                    leftJoystickCentreX + joystickRadius,
                    joystickCentreY + joystickRadius);
            case RIGHT_AXIS -> new Rect(rightJoystickCentreX - joystickRadius,
                    joystickCentreY - joystickRadius,
                    rightJoystickCentreX + joystickRadius,
                    joystickCentreY + joystickRadius);
            case L_STICK ->
                    new Rect((int) (leftJoystickCentreX + joystickRadius * 1.5f - joystickClickRadius),
                            (int) (joystickCentreY + joystickRadius * 1.5f - joystickClickRadius),
                            (int) (leftJoystickCentreX + joystickRadius * 1.5f + joystickClickRadius),
                            (int) (joystickCentreY + joystickRadius * 1.5f + joystickClickRadius));
            case R_STICK ->
                    new Rect((int) (rightJoystickCentreX - joystickRadius * 1.5f - joystickClickRadius),
                            (int) (joystickCentreY + joystickRadius * 1.5f - joystickClickRadius),
                            (int) (rightJoystickCentreX - joystickRadius * 1.5f + joystickClickRadius),
                            (int) (joystickCentreY + joystickRadius * 1.5f + joystickClickRadius));
        };
    }

    public static class OverlaySettings {
        private int controllerIndex;
        private boolean overlayEnabled;
        private boolean vibrateOnTouchEnabled;
        private int alpha;
        private final Consumer<OverlaySettings> overlaySettingsConsumer;

        private OverlaySettings(boolean vibrateOnTouchEnabled, boolean overlayEnabled, int controllerIndex, int alpha, Consumer<OverlaySettings> overlaySettingsConsumer) {
            setControllerIndex(controllerIndex);
            setAlpha(alpha);
            setVibrateOnTouchEnabled(vibrateOnTouchEnabled);
            setOverlayEnabled(overlayEnabled);
            this.overlaySettingsConsumer = overlaySettingsConsumer;
        }

        public void setAlpha(int alpha) {
            this.alpha = Math.max(0, Math.min(255, alpha));
        }

        public int getAlpha() {
            return this.alpha;
        }

        public void setControllerIndex(int controllerIndex) {
            this.controllerIndex = Math.min(Math.max(controllerIndex, 0), NativeLibrary.MAX_CONTROLLERS - 1);
        }

        public int getControllerIndex() {
            return this.controllerIndex;
        }

        public void saveSettings() {
            overlaySettingsConsumer.accept(this);
        }

        public boolean isOverlayEnabled() {
            return overlayEnabled;
        }

        public void setOverlayEnabled(boolean overlayEnabled) {
            this.overlayEnabled = overlayEnabled;
        }

        public boolean isVibrateOnTouchEnabled() {
            return vibrateOnTouchEnabled;
        }

        public void setVibrateOnTouchEnabled(boolean vibrateOnTouchEnabled) {
            this.vibrateOnTouchEnabled = vibrateOnTouchEnabled;
        }
    }

    public static class InputOverlaySettings {
        private Rect rectangle;
        private final Consumer<InputOverlaySettings> inputOverlaySettingsConsumer;
        private final int alpha;

        private InputOverlaySettings(Rect rectangle, int alpha, Consumer<InputOverlaySettings> inputOverlaySettingsConsumer) {
            setRect(rectangle);
            this.alpha = alpha;
            this.inputOverlaySettingsConsumer = inputOverlaySettingsConsumer;
        }

        public int getAlpha() {
            return alpha;
        }

        public void setRect(Rect rectangle) {
            this.rectangle = rectangle;
        }

        public Rect getRect() {
            return this.rectangle;
        }

        public void saveSettings() {
            inputOverlaySettingsConsumer.accept(this);
        }
    }
}
