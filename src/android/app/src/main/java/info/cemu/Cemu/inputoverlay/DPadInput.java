package info.cemu.Cemu.inputoverlay;

import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.util.Log;
import android.view.MotionEvent;

import androidx.annotation.DrawableRes;
import androidx.core.content.res.ResourcesCompat;

import java.util.Objects;
import java.util.function.Supplier;

public class DPadInput extends Input {
    Drawable iconDpadUp;
    Drawable iconDpadUpPressed;
    Drawable iconDpadUpNotPressed;

    Drawable iconDpadDown;
    Drawable iconDpadDownPressed;
    Drawable iconDpadDownNotPressed;

    Drawable iconDpadLeft;
    Drawable iconDpadLeftPressed;
    Drawable iconDpadLeftNotPressed;

    Drawable iconDpadRight;
    Drawable iconDpadRightPressed;
    Drawable iconDpadRightNotPressed;
    Drawable background;

    enum DpadState {
        NONE, UP, DOWN, LEFT, RIGHT, UP_LEFT, UP_RIGHT, DOWN_LEFT, DOWN_RIGHT,
    }

    DpadState dpadState = DpadState.NONE;

    int centreX;
    int centreY;
    int radius;
    int currentPointerId = -1;

    @FunctionalInterface
    private interface ConfigureButtonInterface {
        void configure(int circleXPos, int circleYPos, Drawable pressedButton, Drawable notPressedButton);
    }

    private final ButtonStateChangeListener buttonStateChangeListener;

    @Override
    protected void configure() {
        var rect = settings.getRect();
        this.centreX = rect.centerX();
        this.centreY = rect.centerY();
        this.radius = Math.min(rect.width(), rect.height()) / 2;

        background.setAlpha(settings.getAlpha());
        background.setBounds(
                centreX - radius,
                centreY - radius,
                centreX + radius,
                centreY + radius);

        int buttonSize = (int) (radius * 0.5f);
        ConfigureButtonInterface configureButton = (circleXPos, circleYPos, pressedButton, notPressedButton) -> {
            pressedButton.setAlpha(settings.getAlpha());
            notPressedButton.setAlpha(settings.getAlpha());
            int left = circleXPos - buttonSize / 2;
            int top = circleYPos - buttonSize / 2;
            pressedButton.setBounds(left, top, left + buttonSize, top + buttonSize);
            notPressedButton.setBounds(left, top, left + buttonSize, top + buttonSize);
        };
        configureButton.configure(
                centreX,
                centreY - 2 * radius / 3,
                iconDpadUpPressed,
                iconDpadUp
        );
        configureButton.configure(
                centreX,
                centreY + 2 * radius / 3,
                iconDpadDownPressed,
                iconDpadDownNotPressed
        );
        configureButton.configure(
                centreX - 2 * radius / 3,
                centreY,
                iconDpadLeftPressed,
                iconDpadLeftNotPressed
        );
        configureButton.configure(
                centreX + 2 * radius / 3,
                centreY,
                iconDpadRightPressed,
                iconDpadRightNotPressed
        );
    }

    public DPadInput(Resources resources, @DrawableRes int backgroundId, @DrawableRes int pressedButtonId, @DrawableRes int notPressedButtonId, ButtonStateChangeListener buttonStateChangeListener, InputOverlaySettingsProvider.InputOverlaySettings settings) {
        super(settings);
        this.buttonStateChangeListener = buttonStateChangeListener;

        background = Objects.requireNonNull(ResourcesCompat.getDrawable(resources, backgroundId, null));

        Supplier<Drawable> getPressedButtonIcon = () -> Objects.requireNonNull(ResourcesCompat.getDrawable(resources, pressedButtonId, null));
        Supplier<Drawable> getNotPressedButtonIcon = () -> Objects.requireNonNull(ResourcesCompat.getDrawable(resources, notPressedButtonId, null));

        iconDpadUpPressed = getPressedButtonIcon.get();
        iconDpadUpNotPressed = getNotPressedButtonIcon.get();
        iconDpadUp = iconDpadUpNotPressed;

        iconDpadDownPressed = getPressedButtonIcon.get();
        iconDpadDownNotPressed = getNotPressedButtonIcon.get();
        iconDpadDown = iconDpadDownNotPressed;

        iconDpadLeftPressed = getPressedButtonIcon.get();
        iconDpadLeftNotPressed = getNotPressedButtonIcon.get();
        iconDpadLeft = iconDpadLeftNotPressed;

        iconDpadRightPressed = getPressedButtonIcon.get();
        iconDpadRightNotPressed = getNotPressedButtonIcon.get();
        iconDpadRight = iconDpadRightNotPressed;

        configure();
    }

    private void updateState(DpadState nextDpadState) {
        switch (dpadState) {
            case UP -> {
                buttonStateChangeListener.onButtonStateChange(InputOverlaySurfaceView.OverlayButton.DPAD_UP, false);
                iconDpadUp = iconDpadUpNotPressed;
            }
            case DOWN -> {
                buttonStateChangeListener.onButtonStateChange(InputOverlaySurfaceView.OverlayButton.DPAD_DOWN, false);
                iconDpadDown = iconDpadDownNotPressed;
            }
            case LEFT -> {
                buttonStateChangeListener.onButtonStateChange(InputOverlaySurfaceView.OverlayButton.DPAD_LEFT, false);
                iconDpadLeft = iconDpadLeftNotPressed;
            }
            case RIGHT -> {
                buttonStateChangeListener.onButtonStateChange(InputOverlaySurfaceView.OverlayButton.DPAD_RIGHT, false);
                iconDpadRight = iconDpadRightNotPressed;
            }
            case UP_LEFT -> {
                buttonStateChangeListener.onButtonStateChange(InputOverlaySurfaceView.OverlayButton.DPAD_UP, false);
                buttonStateChangeListener.onButtonStateChange(InputOverlaySurfaceView.OverlayButton.DPAD_LEFT, false);
                iconDpadUp = iconDpadUpNotPressed;
                iconDpadLeft = iconDpadLeftNotPressed;
            }
            case UP_RIGHT -> {
                buttonStateChangeListener.onButtonStateChange(InputOverlaySurfaceView.OverlayButton.DPAD_UP, false);
                buttonStateChangeListener.onButtonStateChange(InputOverlaySurfaceView.OverlayButton.DPAD_RIGHT, false);
                iconDpadUp = iconDpadUpNotPressed;
                iconDpadRight = iconDpadRightNotPressed;
            }
            case DOWN_LEFT -> {
                buttonStateChangeListener.onButtonStateChange(InputOverlaySurfaceView.OverlayButton.DPAD_DOWN, false);
                buttonStateChangeListener.onButtonStateChange(InputOverlaySurfaceView.OverlayButton.DPAD_LEFT, false);
                iconDpadDown = iconDpadDownNotPressed;
                iconDpadLeft = iconDpadLeftNotPressed;
            }
            case DOWN_RIGHT -> {
                buttonStateChangeListener.onButtonStateChange(InputOverlaySurfaceView.OverlayButton.DPAD_DOWN, false);
                buttonStateChangeListener.onButtonStateChange(InputOverlaySurfaceView.OverlayButton.DPAD_RIGHT, false);
                iconDpadDown = iconDpadDownNotPressed;
                iconDpadRight = iconDpadRightNotPressed;
            }
        }
        dpadState = nextDpadState;
        switch (nextDpadState) {
            case UP -> {
                buttonStateChangeListener.onButtonStateChange(InputOverlaySurfaceView.OverlayButton.DPAD_UP, true);
                iconDpadUp = iconDpadUpPressed;
            }
            case DOWN -> {
                buttonStateChangeListener.onButtonStateChange(InputOverlaySurfaceView.OverlayButton.DPAD_DOWN, true);
                iconDpadDown = iconDpadDownPressed;
            }
            case LEFT -> {
                buttonStateChangeListener.onButtonStateChange(InputOverlaySurfaceView.OverlayButton.DPAD_LEFT, true);
                iconDpadLeft = iconDpadLeftPressed;
            }
            case RIGHT -> {
                buttonStateChangeListener.onButtonStateChange(InputOverlaySurfaceView.OverlayButton.DPAD_RIGHT, true);
                iconDpadRight = iconDpadRightPressed;
            }
            case UP_LEFT -> {
                buttonStateChangeListener.onButtonStateChange(InputOverlaySurfaceView.OverlayButton.DPAD_UP, true);
                buttonStateChangeListener.onButtonStateChange(InputOverlaySurfaceView.OverlayButton.DPAD_LEFT, true);
                iconDpadUp = iconDpadUpPressed;
                iconDpadLeft = iconDpadLeftPressed;
            }
            case UP_RIGHT -> {
                buttonStateChangeListener.onButtonStateChange(InputOverlaySurfaceView.OverlayButton.DPAD_UP, true);
                buttonStateChangeListener.onButtonStateChange(InputOverlaySurfaceView.OverlayButton.DPAD_RIGHT, true);
                iconDpadUp = iconDpadUpPressed;
                iconDpadRight = iconDpadRightPressed;
            }
            case DOWN_LEFT -> {
                buttonStateChangeListener.onButtonStateChange(InputOverlaySurfaceView.OverlayButton.DPAD_DOWN, true);
                buttonStateChangeListener.onButtonStateChange(InputOverlaySurfaceView.OverlayButton.DPAD_LEFT, true);
                iconDpadDown = iconDpadDownPressed;
                iconDpadLeft = iconDpadLeftPressed;
            }
            case DOWN_RIGHT -> {
                buttonStateChangeListener.onButtonStateChange(InputOverlaySurfaceView.OverlayButton.DPAD_DOWN, true);
                buttonStateChangeListener.onButtonStateChange(InputOverlaySurfaceView.OverlayButton.DPAD_RIGHT, true);
                iconDpadDown = iconDpadDownPressed;
                iconDpadRight = iconDpadRightPressed;
            }
        }
    }

    @Override
    public boolean onTouch(MotionEvent event) {
        DpadState nextState = dpadState;
        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                int pointerIndex = event.getActionIndex();
                int x = (int) event.getX(pointerIndex);
                int y = (int) event.getY(pointerIndex);
                int pointerId = event.getPointerId(pointerIndex);
                if (isInside(x, y)) {
                    currentPointerId = pointerId;
                    double angle = Math.atan2(y - centreY, x - centreX);
                    nextState = getStateByAngle(angle);
                }
            }
            case MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP -> {
                int pointerId = event.getPointerId(event.getActionIndex());
                if (pointerId == currentPointerId) {
                    currentPointerId = -1;
                    nextState = DpadState.NONE;
                }
            }
            case MotionEvent.ACTION_MOVE -> {
                if (currentPointerId == -1) {
                    break;
                }
                for (int i = 0; i < event.getPointerCount(); i++) {
                    if (currentPointerId != event.getPointerId(i)) {
                        continue;
                    }
                    int x = (int) event.getX(i);
                    int y = (int) event.getY(i);
                    int norm2 = ((x - centreX) * (x - centreX) + (y - centreY) * (y - centreY));
                    if (norm2 <= radius * radius * 0.1f) {
                        nextState = DpadState.NONE;
                        break;
                    }
                    double angle = Math.atan2(y - centreY, x - centreX);
                    nextState = getStateByAngle(angle);
                    break;
                }
            }
        }
        if (nextState != dpadState) {
            updateState(nextState);
            return true;
        }
        return false;
    }

    @Override
    protected void resetInput() {
        updateState(DpadState.NONE);
    }

    private DpadState getStateByAngle(double angle) {
        if (-0.125 * Math.PI <= angle && angle < 0.125 * Math.PI) return DpadState.RIGHT;
        if (0.125 * Math.PI <= angle && angle < 0.375 * Math.PI) return DpadState.DOWN_RIGHT;
        if (0.375 * Math.PI <= angle && angle < 0.625 * Math.PI) return DpadState.DOWN;
        if (0.625 * Math.PI <= angle && angle < 0.875 * Math.PI) return DpadState.DOWN_LEFT;
        if (-0.875 * Math.PI <= angle && angle < -0.625 * Math.PI) return DpadState.UP_LEFT;
        if (-0.625 * Math.PI <= angle && angle < -0.375 * Math.PI) return DpadState.UP;
        if (-0.375 * Math.PI <= angle && angle < -0.125 * Math.PI) return DpadState.UP_RIGHT;
        return DpadState.LEFT;
    }

    @Override
    public boolean isInside(int x, int y) {
        return ((x - centreX) * (x - centreX) + (y - centreY) * (y - centreY)) <= radius * radius;
    }

    @Override
    protected void drawInput(Canvas canvas) {
        background.draw(canvas);
        iconDpadUp.draw(canvas);
        iconDpadDown.draw(canvas);
        iconDpadLeft.draw(canvas);
        iconDpadRight.draw(canvas);
    }
}
