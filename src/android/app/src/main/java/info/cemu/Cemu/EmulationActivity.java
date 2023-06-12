package info.cemu.Cemu;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsAnimationCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsControllerCompat;

import android.os.Bundle;

import info.cemu.Cemu.databinding.ActivityEmulationBinding;
import info.cemu.Cemu.databinding.ActivityMainBinding;

public class EmulationActivity extends AppCompatActivity {

    public static final String GAME_TITLE_ID = "GameTitleId";
    long gameTitleId;
    ActivityEmulationBinding binding;
    EmulationFragment emulationFragment;

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
            emulationFragment = EmulationFragment.newInstance(gameTitleId);
            getSupportFragmentManager()
                    .beginTransaction()
                    .add(R.id.emulation_frame, emulationFragment)
                    .commit();
        }
    }

    void setFullscreen() {
        WindowCompat.setDecorFitsSystemWindows(getWindow(), false);
        WindowInsetsControllerCompat controller = new WindowInsetsControllerCompat(getWindow(), getWindow().getDecorView());
        controller.hide(WindowInsetsCompat.Type.systemBars());
        controller.setSystemBarsBehavior(WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
    }
}