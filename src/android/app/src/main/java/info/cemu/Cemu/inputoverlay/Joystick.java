package info.cemu.Cemu.inputoverlay;

import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.view.MotionEvent;

import androidx.annotation.DrawableRes;
import androidx.core.content.res.ResourcesCompat;

import java.util.Objects;

public class Joystick extends Input {
    private final Drawable iconPressed;
    private final Drawable iconNotPressed;
    private final Drawable joystickBackground;
    private Drawable icon;
    private int currentPointerId = -1;
    private int centreX = 0;
    private int centreY = 0;
    private int radius = 0;
    private int innerRadius = 0;

    public interface StickStateChangeListener {
        void onStickStateChange(InputOverlaySurfaceView.OverlayJoystick joystick, float x, float y);
    }

    private final StickStateChangeListener stickStateChangeListener;
    private final InputOverlaySurfaceView.OverlayJoystick joystick;

    public Joystick(Resources resources, @DrawableRes int joystickBackgroundId, @DrawableRes int pressedStickId, @DrawableRes int notPressedStickId, StickStateChangeListener stickStateChangeListener, InputOverlaySurfaceView.OverlayJoystick joystick, InputOverlaySettingsProvider.InputOverlaySettings settings) {
        super(settings);
        joystickBackground = Objects.requireNonNull(ResourcesCompat.getDrawable(resources, joystickBackgroundId, null));
        iconPressed = Objects.requireNonNull(ResourcesCompat.getDrawable(resources, pressedStickId, null));
        iconNotPressed = Objects.requireNonNull(ResourcesCompat.getDrawable(resources, notPressedStickId, null));
        icon = iconNotPressed;
        this.stickStateChangeListener = stickStateChangeListener;
        this.joystick = joystick;
        configure();
    }

    private void updateState(boolean pressed, float x, float y) {
        if (pressed) {
            icon = iconPressed;
        } else {
            icon = iconNotPressed;
        }
        stickStateChangeListener.onStickStateChange(joystick, x, y);
        int newCentreX = (int) (centreX + radius * x);
        int newCentreY = (int) (centreY + radius * y);
        iconPressed.setBounds(newCentreX - innerRadius, newCentreY - innerRadius, newCentreX + innerRadius, newCentreY + innerRadius);
    }

    @Override
    protected void configure() {
        var rect = settings.getRect();
        this.centreX = rect.centerX();
        this.centreY = rect.centerY();
        this.radius = Math.min(rect.width(), rect.height()) / 2;
        this.innerRadius = (int) (radius * 0.65f);
        joystickBackground.setBounds(centreX - radius, centreY - radius, centreX + radius, centreY + radius);
        joystickBackground.setAlpha(settings.getAlpha());
        iconPressed.setAlpha(settings.getAlpha());
        iconPressed.setBounds(centreX - innerRadius, centreY - innerRadius, centreX + innerRadius, centreY + innerRadius);
        iconNotPressed.setAlpha(settings.getAlpha());
        iconNotPressed.setBounds(centreX - innerRadius, centreY - innerRadius, centreX + innerRadius, centreY + innerRadius);
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
                    updateState(true, 0.0f, 0.0f);
                    stateUpdated = true;
                }
            }
            case MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP -> {
                if (currentPointerId == event.getPointerId(event.getActionIndex())) {
                    currentPointerId = -1;
                    updateState(false, 0.0f, 0.0f);
                    stateUpdated = true;
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
                    float x = (event.getX(i) - centreX) / radius;
                    float y = (event.getY(i) - centreY) / radius;
                    float norm = (float) Math.sqrt(x * x + y * y);
                    if (norm > 1.0f) {
                        x /= norm;
                        y /= norm;
                    }
                    updateState(true, x, y);
                    stateUpdated = true;
                    break;
                }
            }
        }
        return stateUpdated;
    }

    @Override
    protected void resetInput() {
        updateState(false, 0, 0);
    }

    @Override
    protected void drawInput(Canvas canvas) {
        joystickBackground.draw(canvas);
        icon.draw(canvas);
    }

    @Override
    public boolean isInside(int x, int y) {
        return ((x - centreX) * (x - centreX) + (y - centreY) * (y - centreY)) <= radius * radius;
    }

}
