package info.cemu.Cemu.inputoverlay;



import static info.cemu.Cemu.inputoverlay.InputOverlaySurfaceView.InputMode.EDIT_POSITION;
import static info.cemu.Cemu.inputoverlay.InputOverlaySurfaceView.InputMode.EDIT_SIZE;

import android.os.Bundle;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsControllerCompat;

import info.cemu.Cemu.R;
import info.cemu.Cemu.databinding.ActivityOverlayConfigureBinding;

public class InputOverlayConfigureActivity extends AppCompatActivity {
    private Toast toast;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setFullscreen();
        ActivityOverlayConfigureBinding binding = ActivityOverlayConfigureBinding.inflate(getLayoutInflater());
        InputOverlaySurfaceView surfaceView = binding.inputConfigureOverlay;
        surfaceView.setInputMode(EDIT_POSITION);
        binding.configureModeButton.setOnClickListener(v -> {
            InputOverlaySurfaceView.InputMode nextMode;
            int toastTextResId;
            if (surfaceView.getInputMode() == EDIT_POSITION) {
                toastTextResId = R.string.input_mode_edit_size;
                nextMode = EDIT_SIZE;
            } else {
                toastTextResId = R.string.input_mode_edit_position;
                nextMode = EDIT_POSITION;
            }
            if (toast != null) {
                toast.cancel();
            }
            toast = Toast.makeText(InputOverlayConfigureActivity.this, toastTextResId, Toast.LENGTH_SHORT);
            toast.show();
            surfaceView.setInputMode(nextMode);
        });
        binding.resetOverlayButton.setOnClickListener(v -> surfaceView.resetInputs());
        setContentView(binding.getRoot());
    }

    private void setFullscreen() {
        WindowCompat.setDecorFitsSystemWindows(getWindow(), false);
        WindowInsetsControllerCompat controller = new WindowInsetsControllerCompat(getWindow(), getWindow().getDecorView());
        controller.hide(WindowInsetsCompat.Type.systemBars());
        controller.setSystemBarsBehavior(WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
    }
}