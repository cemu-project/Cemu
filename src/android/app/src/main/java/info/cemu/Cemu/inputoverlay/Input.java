package info.cemu.Cemu.inputoverlay;

import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.view.MotionEvent;

import androidx.annotation.ColorInt;

public abstract class Input {
    protected final InputOverlaySettingsProvider.InputOverlaySettings settings;
    private boolean drawBoundingRectangle;
    private Rect boundingRectangle;
    private final Paint boundingRectanglePaint = new Paint();

    protected Input(InputOverlaySettingsProvider.InputOverlaySettings settings) {
        this.settings = settings;
        boundingRectangle = settings.getRect();
    }

    public abstract boolean onTouch(MotionEvent event);

    public void moveInput(int x, int y, int maxWidth, int maxHeight) {
        var rect = settings.getRect();
        int width = rect.width();
        int height = rect.height();
        int left = Math.min(Math.max(x - width / 2, 0), maxWidth - width);
        int top = Math.min(Math.max(y - height / 2, 0), maxHeight - height);
        boundingRectangle = new Rect(
                left,
                top,
                left + width,
                top + height
        );
        settings.setRect(boundingRectangle);
        configure();
    }

    private static final float MIN_WIDTH_HEIGHT = 100;

    public void resize(int diffX, int diffY, int maxWidth, int maxHeight) {
        var rect = settings.getRect();
        int newRight = rect.right + diffX;
        int newBottom = rect.bottom + diffY;
        if (newRight - rect.left < MIN_WIDTH_HEIGHT || newBottom - rect.top < MIN_WIDTH_HEIGHT
                || newRight > maxWidth || newBottom > maxHeight) {
            return;
        }
        boundingRectangle = new Rect(
                rect.left,
                rect.top,
                newRight,
                newBottom
        );
        settings.setRect(boundingRectangle);
        configure();
    }

    protected abstract void resetInput();

    public void reset() {
        drawBoundingRectangle = false;
    }

    public void saveConfiguration() {
        settings.saveSettings();
    }

    protected abstract void configure();

    protected abstract void drawInput(Canvas canvas);

    public void draw(Canvas canvas) {
        if (drawBoundingRectangle) {
            canvas.drawRect(boundingRectangle, boundingRectanglePaint);
        }
        drawInput(canvas);
    }

    public void enableDrawingBoundingRect(@ColorInt int color) {
        drawBoundingRectangle = true;
        boundingRectanglePaint.setColor(color);
    }

    public void disableDrawingBoundingRect() {
        drawBoundingRectangle = false;
    }

    public abstract boolean isInside(int x, int y);
}
