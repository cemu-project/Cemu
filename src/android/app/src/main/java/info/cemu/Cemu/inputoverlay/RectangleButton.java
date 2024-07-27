package info.cemu.Cemu.inputoverlay;

import android.content.res.Resources;

import androidx.annotation.DrawableRes;

public class RectangleButton extends Button {
    int left;
    int top;
    int right;
    int bottom;

    public RectangleButton(Resources resources, @DrawableRes int pressedButtonId, @DrawableRes int notPressedButtonId, ButtonStateChangeListener buttonStateChangeListener, InputOverlaySurfaceView.OverlayButton overlayButton, InputOverlaySettingsProvider.InputOverlaySettings settings) {
        super(resources, pressedButtonId, notPressedButtonId, buttonStateChangeListener, overlayButton, settings);
        configure();
    }

    @Override
    protected void configure() {
        var rect = settings.getRect();
        this.left = rect.left;
        this.top = rect.top;
        this.right = rect.right;
        this.bottom = rect.bottom;
        iconPressed.setBounds(left, top, right, bottom);
        iconNotPressed.setBounds(left, top, right, bottom);
    }

    @Override
    public boolean isInside(int x, int y) {
        return x >= left && x < right && y >= top && y < bottom;
    }

}
