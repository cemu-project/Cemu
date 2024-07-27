package info.cemu.Cemu.settings.input;

import android.app.AlertDialog;
import android.content.Context;
import android.view.MotionEvent;
import android.view.View;

import androidx.annotation.NonNull;

public class MotionDialog extends AlertDialog {
    public static interface OnMotionListener {
        boolean onMotion(MotionEvent ev);
    }

    private OnMotionListener onMotionListener;

    protected MotionDialog(Context context) {
        super(context);
    }

    public void setOnMotionListener(OnMotionListener onMotionListener) {
        this.onMotionListener = onMotionListener;
    }

    @Override
    public boolean onGenericMotionEvent(@NonNull MotionEvent event) {
        if (onMotionListener != null && onMotionListener.onMotion(event)) {
            return true;
        }
        return super.onGenericMotionEvent(event);
    }
}

