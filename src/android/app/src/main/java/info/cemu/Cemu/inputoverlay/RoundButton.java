package info.cemu.Cemu.inputoverlay;

import android.content.res.Resources;
import android.graphics.Rect;

import androidx.annotation.DrawableRes;

public class RoundButton extends Button {
    int centreX;
    int centreY;
    int radius;

    RoundButton(Resources resources, @DrawableRes int pressedButtonId, @DrawableRes int notPressedButtonId, ButtonStateChangeListener buttonStateChangeListener, InputOverlaySurfaceView.OverlayButton overlayButton, InputOverlaySettingsProvider.InputOverlaySettings settings) {
        super(resources, pressedButtonId, notPressedButtonId, buttonStateChangeListener, overlayButton, settings);
        configure();
    }

    @Override
    protected void configure() {
        var rect = settings.getRect();
        centreX = rect.centerX();
        centreY = rect.centerY();
        radius = Math.min(rect.width(), rect.height()) / 2;
        var iconRect = new Rect(
                centreX - radius,
                centreY - radius,
                centreX + radius,
                centreY + radius);
        iconPressed.setBounds(iconRect);
        iconNotPressed.setBounds(iconRect);
    }

    @Override
    public boolean isInside(int x, int y) {
        return ((x - centreX) * (x - centreX) + (y - centreY) * (y - centreY)) <= radius * radius;
    }

}
