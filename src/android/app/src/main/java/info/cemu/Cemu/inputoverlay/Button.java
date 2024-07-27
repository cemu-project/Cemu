package info.cemu.Cemu.inputoverlay;

import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.view.MotionEvent;

import androidx.annotation.DrawableRes;
import androidx.core.content.res.ResourcesCompat;

import java.util.Objects;

public abstract class Button extends Input {
    protected Drawable iconPressed;
    protected Drawable iconNotPressed;
    protected Drawable icon;
    protected int currentPointerId = -1;
    private final ButtonStateChangeListener buttonStateChangeListener;
    private final InputOverlaySurfaceView.OverlayButton button;

    public Button(Resources resources, @DrawableRes int pressedButtonId, @DrawableRes int notPressedButtonId, ButtonStateChangeListener buttonStateChangeListener, InputOverlaySurfaceView.OverlayButton button, InputOverlaySettingsProvider.InputOverlaySettings settings) {
        super(settings);
        iconPressed = Objects.requireNonNull(ResourcesCompat.getDrawable(resources, pressedButtonId, null));
        iconPressed.setAlpha(settings.getAlpha());
        iconNotPressed = Objects.requireNonNull(ResourcesCompat.getDrawable(resources, notPressedButtonId, null));
        iconNotPressed.setAlpha(settings.getAlpha());
        icon = iconNotPressed;
        this.buttonStateChangeListener = buttonStateChangeListener;
        this.button = button;
    }

    @Override
    public boolean onTouch(MotionEvent event) {
        boolean stateUpdated = false;
        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                int pointerIndex = event.getActionIndex();
                int x = (int) event.getX(pointerIndex);
                int y = (int) event.getY(pointerIndex);
                if (isInside(x, y)) {
                    currentPointerId = event.getPointerId(pointerIndex);
                    updateState(true);
                    stateUpdated = true;
                }
            }
            case MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP -> {
                if (event.getPointerId(event.getActionIndex()) == currentPointerId) {
                    currentPointerId = -1;
                    updateState(false);
                    stateUpdated = true;
                }
            }
        }
        return stateUpdated;
    }

    @Override
    protected void drawInput(Canvas canvas) {
        icon.draw(canvas);
    }

    @Override
    protected void resetInput() {
        updateState(false);
    }

    protected void updateState(boolean state) {
        if (state) {
            icon = iconPressed;
        } else {
            icon = iconNotPressed;
        }
        buttonStateChangeListener.onButtonStateChange(button, state);
    }
}
