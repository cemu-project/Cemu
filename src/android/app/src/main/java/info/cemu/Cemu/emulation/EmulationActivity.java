package info.cemu.Cemu.emulation;

import android.os.Bundle;
import android.view.KeyEvent;
import android.view.MotionEvent;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsControllerCompat;

import com.google.android.material.dialog.MaterialAlertDialogBuilder;

import info.cemu.Cemu.R;
import info.cemu.Cemu.databinding.ActivityEmulationBinding;
import info.cemu.Cemu.input.InputManager;

public class EmulationActivity extends AppCompatActivity {

    public static final String GAME_TITLE_ID = "GameTitleId";
    private long gameTitleId;
    private ActivityEmulationBinding binding;
    private EmulationFragment emulationFragment;
    private final InputManager inputManager = new InputManager();

    @Override
    public boolean onGenericMotionEvent(MotionEvent event) {
        if (inputManager.onMotionEvent(event))
            return true;
        return super.onGenericMotionEvent(event);
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (inputManager.onKeyEvent(event))
            return true;
        return super.dispatchKeyEvent(event);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Bundle extras = getIntent().getExtras();
        if (extras != null) {
            gameTitleId = extras.getLong(GAME_TITLE_ID);
        }
        setFullscreen();
        binding = ActivityEmulationBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());
        emulationFragment = (EmulationFragment) getSupportFragmentManager().findFragmentById(R.id.emulation_frame);
        if (emulationFragment == null) {
            emulationFragment = new EmulationFragment(gameTitleId);
            getSupportFragmentManager()
                    .beginTransaction()
                    .add(R.id.emulation_frame, emulationFragment)
                    .commit();
        }
    }

    @Override
    public void onBackPressed() {
        showExitConfirmationDialog();
    }

    private void showExitConfirmationDialog() {
        MaterialAlertDialogBuilder builder = new MaterialAlertDialogBuilder(this);
        builder.setTitle(R.string.exit_confirmation_title)
                .setMessage(R.string.exit_confirm_message)
                .setPositiveButton(R.string.yes, (dialog, which) -> {
                    finishAffinity();
                    System.exit(0);
                }).setNegativeButton(R.string.no, (dialog, which) -> dialog.cancel())
                .create()
                .show();
    }

    private void setFullscreen() {
        WindowCompat.setDecorFitsSystemWindows(getWindow(), false);
        WindowInsetsControllerCompat controller = new WindowInsetsControllerCompat(getWindow(), getWindow().getDecorView());
        controller.hide(WindowInsetsCompat.Type.systemBars());
        controller.setSystemBarsBehavior(WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
    }
}