package info.cemu.Cemu.inputoverlay;


import android.content.Context;
import android.graphics.Canvas;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.SurfaceView;
import android.view.View;

import java.util.ArrayList;
import java.util.List;

import info.cemu.Cemu.R;
import info.cemu.Cemu.NativeLibrary;

public class InputOverlaySurfaceView extends SurfaceView implements View.OnTouchListener {
    public enum InputMode {
        DEFAULT,
        EDIT_POSITION,
        EDIT_SIZE,
    }

    private InputMode inputMode = InputMode.DEFAULT;

    public void resetInputs() {
        int width = getWidth();
        int height = getHeight();
        for (var input : InputOverlaySettingsProvider.Input.values()) {
            var inputSettings = settingsProvider.getOverlaySettingsForInput(input, width, height);
            inputSettings.setRect(settingsProvider.getDefaultRectangle(input, width, height));
            inputSettings.saveSettings();
        }
        inputs.clear();
        inputs = null;
        setInputs();
        invalidate();
    }

    public void setInputMode(InputMode inputMode) {
        if (inputs != null) {
            if (this.inputMode != InputMode.DEFAULT) {
                for (var input : inputs) {
                    input.saveConfiguration();
                }
            } else {
                for (var input : inputs) {
                    input.reset();
                }
            }
        }
        this.inputMode = inputMode;
    }

    public InputMode getInputMode() {
        return inputMode;
    }

    public enum OverlayButton {
        A,
        B,
        ONE,
        TWO,
        C,
        Z,
        HOME,
        DPAD_DOWN,
        DPAD_LEFT,
        DPAD_RIGHT,
        DPAD_UP,
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
    }


    public enum OverlayJoystick {
        LEFT,
        RIGHT,
    }

    private int nativeControllerType = -1;
    int controllerIndex;

    private List<Input> inputs;
    private final InputOverlaySettingsProvider settingsProvider;
    private final Vibrator vibrator;
    private final VibrationEffect buttonTouchVibrationEffect = VibrationEffect.createPredefined(VibrationEffect.EFFECT_TICK);
    private final boolean vibrateOnTouch;

    public InputOverlaySurfaceView(Context context, AttributeSet attrs) {
        super(context, attrs);
        context.getSystemService(Context.VIBRATOR_SERVICE);
        vibrator = (Vibrator) context.getSystemService(Context.VIBRATOR_SERVICE);
        setOnTouchListener(this);
        settingsProvider = new InputOverlaySettingsProvider(context);
        var overlaySettings = settingsProvider.getOverlaySettings();
        controllerIndex = overlaySettings.getControllerIndex();
        vibrateOnTouch = vibrator != null && vibrator.hasVibrator() && overlaySettings.isVibrateOnTouchEnabled();
    }

    int overlayButtonToVPADButton(OverlayButton button) {
        return switch (button) {
            case A -> NativeLibrary.VPAD_BUTTON_A;
            case B -> NativeLibrary.VPAD_BUTTON_B;
            case X -> NativeLibrary.VPAD_BUTTON_X;
            case Y -> NativeLibrary.VPAD_BUTTON_Y;
            case PLUS -> NativeLibrary.VPAD_BUTTON_PLUS;
            case MINUS -> NativeLibrary.VPAD_BUTTON_MINUS;
            case DPAD_UP -> NativeLibrary.VPAD_BUTTON_UP;
            case DPAD_DOWN -> NativeLibrary.VPAD_BUTTON_DOWN;
            case DPAD_LEFT -> NativeLibrary.VPAD_BUTTON_LEFT;
            case DPAD_RIGHT -> NativeLibrary.VPAD_BUTTON_RIGHT;
            case L_STICK -> NativeLibrary.VPAD_BUTTON_STICKL;
            case R_STICK -> NativeLibrary.VPAD_BUTTON_STICKR;
            case L -> NativeLibrary.VPAD_BUTTON_L;
            case R -> NativeLibrary.VPAD_BUTTON_R;
            case ZR -> NativeLibrary.VPAD_BUTTON_ZR;
            case ZL -> NativeLibrary.VPAD_BUTTON_ZL;
            default -> -1;
        };
    }

    int overlayButtonToClassicButton(OverlayButton button) {
        return switch (button) {
            case A -> NativeLibrary.CLASSIC_BUTTON_A;
            case B -> NativeLibrary.CLASSIC_BUTTON_B;
            case X -> NativeLibrary.CLASSIC_BUTTON_X;
            case Y -> NativeLibrary.CLASSIC_BUTTON_Y;
            case PLUS -> NativeLibrary.CLASSIC_BUTTON_PLUS;
            case MINUS -> NativeLibrary.CLASSIC_BUTTON_MINUS;
            case DPAD_UP -> NativeLibrary.CLASSIC_BUTTON_UP;
            case DPAD_DOWN -> NativeLibrary.CLASSIC_BUTTON_DOWN;
            case DPAD_LEFT -> NativeLibrary.CLASSIC_BUTTON_LEFT;
            case DPAD_RIGHT -> NativeLibrary.CLASSIC_BUTTON_RIGHT;
            case L -> NativeLibrary.CLASSIC_BUTTON_L;
            case R -> NativeLibrary.CLASSIC_BUTTON_R;
            case ZR -> NativeLibrary.CLASSIC_BUTTON_ZR;
            case ZL -> NativeLibrary.CLASSIC_BUTTON_ZL;
            default -> -1;
        };
    }

    int overlayButtonToProButton(OverlayButton button) {
        return switch (button) {
            case A -> NativeLibrary.PRO_BUTTON_A;
            case B -> NativeLibrary.PRO_BUTTON_B;
            case X -> NativeLibrary.PRO_BUTTON_X;
            case Y -> NativeLibrary.PRO_BUTTON_Y;
            case PLUS -> NativeLibrary.PRO_BUTTON_PLUS;
            case MINUS -> NativeLibrary.PRO_BUTTON_MINUS;
            case DPAD_UP -> NativeLibrary.PRO_BUTTON_UP;
            case DPAD_DOWN -> NativeLibrary.PRO_BUTTON_DOWN;
            case DPAD_LEFT -> NativeLibrary.PRO_BUTTON_LEFT;
            case DPAD_RIGHT -> NativeLibrary.PRO_BUTTON_RIGHT;
            case L_STICK -> NativeLibrary.PRO_BUTTON_STICKL;
            case R_STICK -> NativeLibrary.PRO_BUTTON_STICKR;
            case L -> NativeLibrary.PRO_BUTTON_L;
            case R -> NativeLibrary.PRO_BUTTON_R;
            case ZR -> NativeLibrary.PRO_BUTTON_ZR;
            case ZL -> NativeLibrary.PRO_BUTTON_ZL;
            default -> -1;
        };
    }

    int overlayButtonToWiimoteButton(OverlayButton button) {
        return switch (button) {
            case A -> NativeLibrary.WIIMOTE_BUTTON_A;
            case B -> NativeLibrary.WIIMOTE_BUTTON_B;
            case ONE -> NativeLibrary.WIIMOTE_BUTTON_1;
            case TWO -> NativeLibrary.WIIMOTE_BUTTON_2;
            case PLUS -> NativeLibrary.WIIMOTE_BUTTON_PLUS;
            case MINUS -> NativeLibrary.WIIMOTE_BUTTON_MINUS;
            case HOME -> NativeLibrary.WIIMOTE_BUTTON_HOME;
            case DPAD_UP -> NativeLibrary.WIIMOTE_BUTTON_UP;
            case DPAD_DOWN -> NativeLibrary.WIIMOTE_BUTTON_DOWN;
            case DPAD_LEFT -> NativeLibrary.WIIMOTE_BUTTON_LEFT;
            case DPAD_RIGHT -> NativeLibrary.WIIMOTE_BUTTON_RIGHT;
            case C -> NativeLibrary.WIIMOTE_BUTTON_NUNCHUCK_C;
            case Z -> NativeLibrary.WIIMOTE_BUTTON_NUNCHUCK_Z;
            default -> -1;
        };
    }

    void onButtonStateChange(OverlayButton button, boolean state) {
        int nativeButtonId = switch (nativeControllerType) {
            case NativeLibrary.EMULATED_CONTROLLER_TYPE_VPAD -> overlayButtonToVPADButton(button);
            case NativeLibrary.EMULATED_CONTROLLER_TYPE_CLASSIC ->
                    overlayButtonToClassicButton(button);
            case NativeLibrary.EMULATED_CONTROLLER_TYPE_PRO -> overlayButtonToProButton(button);
            case NativeLibrary.EMULATED_CONTROLLER_TYPE_WIIMOTE ->
                    overlayButtonToWiimoteButton(button);
            default -> -1;
        };
        if (nativeButtonId == -1) {
            return;
        }
        if (vibrateOnTouch && state) {
            vibrator.vibrate(buttonTouchVibrationEffect);
        }
        NativeLibrary.onOverlayButton(controllerIndex, nativeButtonId, state);
    }

    void onOverlayAxis(int axis, float value) {
        NativeLibrary.onOverlayAxis(controllerIndex, axis, value);
    }

    void onVPADJoystickStateChange(OverlayJoystick joystick, float up, float down, float left, float right) {
        if (joystick == OverlayJoystick.LEFT) {
            onOverlayAxis(NativeLibrary.VPAD_BUTTON_STICKL_UP, up);
            onOverlayAxis(NativeLibrary.VPAD_BUTTON_STICKL_DOWN, down);
            onOverlayAxis(NativeLibrary.VPAD_BUTTON_STICKL_LEFT, left);
            onOverlayAxis(NativeLibrary.VPAD_BUTTON_STICKL_RIGHT, right);
        } else {
            onOverlayAxis(NativeLibrary.VPAD_BUTTON_STICKR_UP, up);
            onOverlayAxis(NativeLibrary.VPAD_BUTTON_STICKR_DOWN, down);
            onOverlayAxis(NativeLibrary.VPAD_BUTTON_STICKR_LEFT, left);
            onOverlayAxis(NativeLibrary.VPAD_BUTTON_STICKR_RIGHT, right);
        }
    }

    void onProJoystickStateChange(OverlayJoystick joystick, float up, float down, float left, float right) {
        if (joystick == OverlayJoystick.LEFT) {
            onOverlayAxis(NativeLibrary.PRO_BUTTON_STICKL_UP, up);
            onOverlayAxis(NativeLibrary.PRO_BUTTON_STICKL_DOWN, down);
            onOverlayAxis(NativeLibrary.PRO_BUTTON_STICKL_LEFT, left);
            onOverlayAxis(NativeLibrary.PRO_BUTTON_STICKL_RIGHT, right);
        } else {
            onOverlayAxis(NativeLibrary.PRO_BUTTON_STICKR_UP, up);
            onOverlayAxis(NativeLibrary.PRO_BUTTON_STICKR_DOWN, down);
            onOverlayAxis(NativeLibrary.PRO_BUTTON_STICKR_LEFT, left);
            onOverlayAxis(NativeLibrary.PRO_BUTTON_STICKR_RIGHT, right);
        }
    }

    void onClassicJoystickStateChange(OverlayJoystick joystick, float up, float down, float left, float right) {
        if (joystick == OverlayJoystick.LEFT) {
            onOverlayAxis(NativeLibrary.CLASSIC_BUTTON_STICKL_UP, up);
            onOverlayAxis(NativeLibrary.CLASSIC_BUTTON_STICKL_DOWN, down);
            onOverlayAxis(NativeLibrary.CLASSIC_BUTTON_STICKL_LEFT, left);
            onOverlayAxis(NativeLibrary.CLASSIC_BUTTON_STICKL_RIGHT, right);
        } else {
            onOverlayAxis(NativeLibrary.CLASSIC_BUTTON_STICKR_UP, up);
            onOverlayAxis(NativeLibrary.CLASSIC_BUTTON_STICKR_DOWN, down);
            onOverlayAxis(NativeLibrary.CLASSIC_BUTTON_STICKR_LEFT, left);
            onOverlayAxis(NativeLibrary.CLASSIC_BUTTON_STICKR_RIGHT, right);
        }
    }

    void onWiimoteJoystickStateChange(OverlayJoystick joystick, float up, float down, float left, float right) {
        if (joystick == OverlayJoystick.LEFT) {
            onOverlayAxis(NativeLibrary.WIIMOTE_BUTTON_NUNCHUCK_UP, up);
            onOverlayAxis(NativeLibrary.WIIMOTE_BUTTON_NUNCHUCK_DOWN, down);
            onOverlayAxis(NativeLibrary.WIIMOTE_BUTTON_NUNCHUCK_LEFT, left);
            onOverlayAxis(NativeLibrary.WIIMOTE_BUTTON_NUNCHUCK_RIGHT, right);
        }
    }

    void onJoystickStateChange(OverlayJoystick joystick, float x, float y) {
        float up = y < 0 ? -y : 0;
        float down = y > 0 ? y : 0;
        float left = x < 0 ? -x : 0;
        float right = x > 0 ? x : 0;
        switch (nativeControllerType) {
            case NativeLibrary.EMULATED_CONTROLLER_TYPE_VPAD ->
                    onVPADJoystickStateChange(joystick, up, down, left, right);
            case NativeLibrary.EMULATED_CONTROLLER_TYPE_PRO ->
                    onProJoystickStateChange(joystick, up, down, left, right);
            case NativeLibrary.EMULATED_CONTROLLER_TYPE_CLASSIC ->
                    onClassicJoystickStateChange(joystick, up, down, left, right);
            case NativeLibrary.EMULATED_CONTROLLER_TYPE_WIIMOTE ->
                    onWiimoteJoystickStateChange(joystick, up, down, left, right);
        }
    }

    private InputOverlaySettingsProvider.InputOverlaySettings getOverlaySettingsForInput(InputOverlaySettingsProvider.Input input) {
        return settingsProvider.getOverlaySettingsForInput(input, getWidth(), getHeight());
    }

    void setInputs() {
        if (inputs != null) {
            return;
        }
        inputs = new ArrayList<>();
        if (NativeLibrary.isControllerDisabled(controllerIndex)) {
            return;
        }
        var resources = getResources();
        nativeControllerType = NativeLibrary.getControllerType(controllerIndex);

        inputs.add(new RoundButton(
                resources,
                R.drawable.button_a_pressed,
                R.drawable.button_a,
                InputOverlaySurfaceView.this::onButtonStateChange,
                OverlayButton.A,
                getOverlaySettingsForInput(InputOverlaySettingsProvider.Input.A)
        ));
        inputs.add(new RoundButton(
                resources,
                R.drawable.button_b_pressed,
                R.drawable.button_b,
                InputOverlaySurfaceView.this::onButtonStateChange,
                OverlayButton.B,
                getOverlaySettingsForInput(InputOverlaySettingsProvider.Input.B)
        ));

        if (nativeControllerType != NativeLibrary.EMULATED_CONTROLLER_TYPE_WIIMOTE) {
            inputs.add(new RoundButton(
                    resources,
                    R.drawable.button_x_pressed,
                    R.drawable.button_x,
                    InputOverlaySurfaceView.this::onButtonStateChange,
                    OverlayButton.X,
                    getOverlaySettingsForInput(InputOverlaySettingsProvider.Input.X)
            ));
            inputs.add(new RoundButton(
                    resources,
                    R.drawable.button_y_pressed,
                    R.drawable.button_y,
                    InputOverlaySurfaceView.this::onButtonStateChange,
                    OverlayButton.Y,
                    getOverlaySettingsForInput(InputOverlaySettingsProvider.Input.Y)
            ));
            inputs.add(new RectangleButton(
                    resources,
                    R.drawable.button_zl_pressed,
                    R.drawable.button_zl,
                    InputOverlaySurfaceView.this::onButtonStateChange,
                    OverlayButton.ZL,
                    getOverlaySettingsForInput(InputOverlaySettingsProvider.Input.ZL)
            ));
            inputs.add(new RectangleButton(
                    resources,
                    R.drawable.button_l_pressed,
                    R.drawable.button_l,
                    InputOverlaySurfaceView.this::onButtonStateChange,
                    OverlayButton.L,
                    getOverlaySettingsForInput(InputOverlaySettingsProvider.Input.L)
            ));
            inputs.add(new RectangleButton(
                    resources,
                    R.drawable.button_zr_pressed,
                    R.drawable.button_zr,
                    InputOverlaySurfaceView.this::onButtonStateChange,
                    OverlayButton.ZR,
                    getOverlaySettingsForInput(InputOverlaySettingsProvider.Input.ZR)
            ));
            inputs.add(new RectangleButton(
                    resources,
                    R.drawable.button_r_pressed,
                    R.drawable.button_r,
                    InputOverlaySurfaceView.this::onButtonStateChange,
                    OverlayButton.R,
                    getOverlaySettingsForInput(InputOverlaySettingsProvider.Input.R)
            ));
            inputs.add(new Joystick(
                    resources,
                    R.drawable.stick_background,
                    R.drawable.stick_inner_pressed,
                    R.drawable.stick_inner,
                    InputOverlaySurfaceView.this::onJoystickStateChange,
                    OverlayJoystick.RIGHT,
                    getOverlaySettingsForInput(InputOverlaySettingsProvider.Input.RIGHT_AXIS)
            ));
        }

        inputs.add(new RoundButton(
                resources,
                R.drawable.button_minus_pressed,
                R.drawable.button_minus,
                InputOverlaySurfaceView.this::onButtonStateChange,
                OverlayButton.MINUS,
                getOverlaySettingsForInput(InputOverlaySettingsProvider.Input.MINUS)
        ));
        inputs.add(new RoundButton(
                resources,
                R.drawable.button_plus_pressed,
                R.drawable.button_plus,
                InputOverlaySurfaceView.this::onButtonStateChange,
                OverlayButton.PLUS,
                getOverlaySettingsForInput(InputOverlaySettingsProvider.Input.PLUS)
        ));

        inputs.add(new DPadInput(
                resources,
                R.drawable.dpad_background,
                R.drawable.dpad_button_pressed,
                R.drawable.dpad_button,
                InputOverlaySurfaceView.this::onButtonStateChange,
                getOverlaySettingsForInput(InputOverlaySettingsProvider.Input.DPAD)
        ));
        inputs.add(new Joystick(
                resources,
                R.drawable.stick_background,
                R.drawable.stick_inner_pressed,
                R.drawable.stick_inner,
                InputOverlaySurfaceView.this::onJoystickStateChange,
                OverlayJoystick.LEFT,
                getOverlaySettingsForInput(InputOverlaySettingsProvider.Input.LEFT_AXIS)
        ));

        if (nativeControllerType == NativeLibrary.EMULATED_CONTROLLER_TYPE_WIIMOTE) {
            inputs.add(new RoundButton(
                    resources,
                    R.drawable.button_c_pressed,
                    R.drawable.button_c,
                    InputOverlaySurfaceView.this::onButtonStateChange,
                    OverlayButton.C,
                    getOverlaySettingsForInput(InputOverlaySettingsProvider.Input.C)
            ));
            inputs.add(new RoundButton(
                    resources,
                    R.drawable.button_z_pressed,
                    R.drawable.button_z,
                    InputOverlaySurfaceView.this::onButtonStateChange,
                    OverlayButton.Z,
                    getOverlaySettingsForInput(InputOverlaySettingsProvider.Input.Z)
            ));
            inputs.add(new RoundButton(
                    resources,
                    R.drawable.button_home_pressed,
                    R.drawable.button_home,
                    InputOverlaySurfaceView.this::onButtonStateChange,
                    OverlayButton.HOME,
                    getOverlaySettingsForInput(InputOverlaySettingsProvider.Input.HOME)
            ));
        }
        if (nativeControllerType != NativeLibrary.EMULATED_CONTROLLER_TYPE_CLASSIC
                && nativeControllerType != NativeLibrary.EMULATED_CONTROLLER_TYPE_WIIMOTE) {
            inputs.add(new RoundButton(
                    resources,
                    R.drawable.button_stick_pressed,
                    R.drawable.button_stick,
                    InputOverlaySurfaceView.this::onButtonStateChange,
                    OverlayButton.L_STICK,
                    getOverlaySettingsForInput(InputOverlaySettingsProvider.Input.L_STICK)
            ));
            inputs.add(new RoundButton(
                    resources,
                    R.drawable.button_stick_pressed,
                    R.drawable.button_stick,
                    InputOverlaySurfaceView.this::onButtonStateChange,
                    OverlayButton.R_STICK,
                    getOverlaySettingsForInput(InputOverlaySettingsProvider.Input.R_STICK)
            ));
        }
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);
        setWillNotDraw(false);
        setInputs();
        requestFocus();
    }

    @Override
    public void draw(Canvas canvas) {
        super.draw(canvas);
        for (var input : inputs) {
            input.draw(canvas);
        }
    }

    Input currentConfiguredInput = null;

    private boolean onEditPosition(MotionEvent event) {
        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN -> {
                int x = (int) event.getX();
                int y = (int) event.getY();
                for (var input : inputs) {
                    if (input.isInside(x, y)) {
                        currentConfiguredInput = input;
                        input.enableDrawingBoundingRect(getResources().getColor(R.color.purple, getContext().getTheme()));
                        return true;
                    }
                }
            }
            case MotionEvent.ACTION_UP -> {
                if (currentConfiguredInput != null) {
                    currentConfiguredInput.disableDrawingBoundingRect();
                    currentConfiguredInput = null;
                    return true;
                }
            }
            case MotionEvent.ACTION_MOVE -> {
                if (currentConfiguredInput != null) {
                    int x = (int) event.getX();
                    int y = (int) event.getY();
                    currentConfiguredInput.moveInput(x, y, getWidth(), getHeight());
                    return true;
                }
            }
        }
        return false;
    }

    private boolean onEditSize(MotionEvent event) {
        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN -> {
                int x = (int) event.getX();
                int y = (int) event.getY();
                for (var input : inputs) {
                    if (input.isInside(x, y)) {
                        currentConfiguredInput = input;
                        input.enableDrawingBoundingRect(getResources().getColor(R.color.red, getContext().getTheme()));
                        return true;
                    }
                }
            }
            case MotionEvent.ACTION_UP -> {
                if (currentConfiguredInput != null) {
                    currentConfiguredInput.disableDrawingBoundingRect();
                    currentConfiguredInput = null;
                    return true;
                }
            }
            case MotionEvent.ACTION_MOVE -> {
                if (currentConfiguredInput != null) {
                    var histSize = event.getHistorySize();
                    if (event.getHistorySize() >= 2) {
                        var x1 = event.getHistoricalX(0);
                        var y1 = event.getHistoricalY(0);
                        var x2 = event.getHistoricalX(histSize - 1);
                        var y2 = event.getHistoricalY(histSize - 1);
                        currentConfiguredInput.resize(
                                (int) (x2 - x1),
                                (int) (y2 - y1),
                                getWidth(),
                                getHeight());
                    }
                    return true;
                }
            }
        }
        return false;
    }

    @Override
    public boolean onTouch(View v, MotionEvent event) {
        boolean touchEventProcessed = false;
        if (inputMode == InputMode.DEFAULT) {
            for (var input : inputs) {
                if (input.onTouch(event)) touchEventProcessed = true;
            }
        } else if (inputMode == InputMode.EDIT_POSITION) {
            touchEventProcessed = onEditPosition(event);
        } else if (inputMode == InputMode.EDIT_SIZE) {
            touchEventProcessed = onEditSize(event);
        }

        if (touchEventProcessed) {
            invalidate();
        }

        return touchEventProcessed;
    }
}
