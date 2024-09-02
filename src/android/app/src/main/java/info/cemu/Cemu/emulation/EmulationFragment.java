package info.cemu.Cemu.emulation;

import android.annotation.SuppressLint;
import android.content.res.Configuration;
import android.graphics.SurfaceTexture;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.appcompat.widget.PopupMenu;
import androidx.fragment.app.Fragment;

import info.cemu.Cemu.NativeLibrary;
import info.cemu.Cemu.R;
import info.cemu.Cemu.databinding.FragmentEmulationBinding;
import info.cemu.Cemu.input.SensorManager;
import info.cemu.Cemu.inputoverlay.InputOverlaySettingsProvider;
import info.cemu.Cemu.inputoverlay.InputOverlaySurfaceView;

@SuppressLint("ClickableViewAccessibility")
public class EmulationFragment extends Fragment implements PopupMenu.OnMenuItemClickListener {
    static private class OnSurfaceTouchListener implements View.OnTouchListener {
        int currentPointerId = -1;
        final boolean isTV;

        public OnSurfaceTouchListener(boolean isTV) {
            this.isTV = isTV;
        }

        @Override
        public boolean onTouch(View v, MotionEvent event) {
            int pointerIndex = event.getActionIndex();
            int pointerId = event.getPointerId(pointerIndex);
            if (currentPointerId != -1 && pointerId != currentPointerId) {
                return false;
            }
            int x = (int) event.getX(pointerIndex);
            int y = (int) event.getY(pointerIndex);
            switch (event.getActionMasked()) {
                case MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                    NativeLibrary.onTouchDown(x, y, isTV);
                    return true;
                }
                case MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP -> {
                    NativeLibrary.onTouchUp(x, y, isTV);
                    return true;
                }
                case MotionEvent.ACTION_MOVE -> {
                    NativeLibrary.onTouchMove(x, y, isTV);
                    return true;
                }
            }
            return false;
        }
    }

    private static class SurfaceHolderCallback implements SurfaceHolder.Callback {
        final boolean isMainCanvas;
        boolean surfaceSet;

        public SurfaceHolderCallback(boolean isMainCanvas) {
            this.isMainCanvas = isMainCanvas;
        }

        @Override
        public void surfaceCreated(@NonNull SurfaceHolder surfaceHolder) {
        }

        @Override
        public void surfaceChanged(@NonNull SurfaceHolder surfaceHolder, int format, int width, int height) {
            NativeLibrary.setSurfaceSize(width, height, isMainCanvas);
            if (!surfaceSet) {
                surfaceSet = true;
                NativeLibrary.setSurface(surfaceHolder.getSurface(), isMainCanvas);
            }
        }

        @Override
        public void surfaceDestroyed(@NonNull SurfaceHolder surfaceHolder) {
            NativeLibrary.clearSurface(isMainCanvas);
            surfaceSet = false;
        }
    }

    private final long gameTitleId;
    private boolean isGameRunning;
    private SurfaceView padCanvas;
    private SurfaceTexture testSurfaceTexture;
    private Surface testSurface;
    private Toast toast;
    private FragmentEmulationBinding binding;
    private boolean isMotionEnabled;
    private PopupMenu settingsMenu;
    private InputOverlaySurfaceView inputOverlaySurfaceView;
    private SensorManager sensorManager;

    public EmulationFragment(long gameTitleId) {
        this.gameTitleId = gameTitleId;
    }

    InputOverlaySettingsProvider.OverlaySettings overlaySettings;

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        var inputOverlaySettingsProvider = new InputOverlaySettingsProvider(requireContext());
        if (sensorManager == null)
            sensorManager = new SensorManager(requireContext());
        sensorManager.setIsLandscape(getResources().getConfiguration().orientation == Configuration.ORIENTATION_LANDSCAPE);
        overlaySettings = inputOverlaySettingsProvider.getOverlaySettings();
        testSurfaceTexture = new SurfaceTexture(0);
        testSurface = new Surface(testSurfaceTexture);
    }

    @Override
    public void onConfigurationChanged(@NonNull Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        if (sensorManager != null)
            sensorManager.setIsLandscape(newConfig.orientation == Configuration.ORIENTATION_LANDSCAPE);
    }

    @Override
    public void onPause() {
        super.onPause();
        sensorManager.pauseListening();
    }

    @Override
    public void onResume() {
        super.onResume();
        if (isMotionEnabled)
            sensorManager.startListening();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        if (sensorManager != null)
            sensorManager.pauseListening();
        if (testSurface != null) testSurface.release();
        if (testSurfaceTexture != null) testSurfaceTexture.release();
    }

    private void createPadCanvas() {
        if (padCanvas != null) return;
        padCanvas = new SurfaceView(requireContext());
        binding.canvasesLayout.addView(
                padCanvas,
                new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.MATCH_PARENT, 1.0f)
        );
        padCanvas.getHolder().addCallback(new SurfaceHolderCallback(false));
        padCanvas.setOnTouchListener(new OnSurfaceTouchListener(false));
    }

    private void toastMessage(@StringRes int toastTextResId) {
        if (toast != null) {
            toast.cancel();
        }
        toast = Toast.makeText(requireContext(), toastTextResId, Toast.LENGTH_SHORT);
        toast.show();
    }

    private void destroyPadCanvas() {
        if (padCanvas == null) return;
        binding.canvasesLayout.removeView(padCanvas);
        padCanvas = null;
    }

    private void setPadViewVisibility(boolean visible) {
        if (visible) {
            createPadCanvas();
            return;
        }
        destroyPadCanvas();
    }

    @Override
    public boolean onMenuItemClick(MenuItem item) {
        int itemId = item.getItemId();
        if (itemId == R.id.show_pad) {
            boolean padVisibility = !item.isChecked();
            setPadViewVisibility(padVisibility);
            item.setChecked(padVisibility);
            return true;
        }
        if (itemId == R.id.edit_inputs) {
            binding.editInputsLayout.setVisibility(View.VISIBLE);
            binding.finishEditInputsButton.setVisibility(View.VISIBLE);
            binding.moveInputsButton.performClick();
            return true;
        }
        if (itemId == R.id.replace_tv_with_pad) {
            boolean replaceTVWithPad = !item.isChecked();
            NativeLibrary.setReplaceTVWithPadView(replaceTVWithPad);
            item.setChecked(replaceTVWithPad);
            return true;
        }
        if (itemId == R.id.reset_inputs) {
            inputOverlaySurfaceView.resetInputs();
            return true;
        }
        if (itemId == R.id.enable_motion) {
            isMotionEnabled = !item.isChecked();
            if (isMotionEnabled)
                sensorManager.startListening();
            else
                sensorManager.pauseListening();
            item.setChecked(isMotionEnabled);
        }
        return false;
    }

    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        binding = FragmentEmulationBinding.inflate(inflater, container, false);
        inputOverlaySurfaceView = binding.inputOverlay;

        binding.moveInputsButton.setOnClickListener(v -> {
            if (inputOverlaySurfaceView.getInputMode() == InputOverlaySurfaceView.InputMode.EDIT_POSITION)
                return;
            binding.resizeInputsButton.setAlpha(0.5f);
            binding.moveInputsButton.setAlpha(1.0f);
            toastMessage(R.string.input_mode_edit_position);
            inputOverlaySurfaceView.setInputMode(InputOverlaySurfaceView.InputMode.EDIT_POSITION);
        });
        binding.resizeInputsButton.setOnClickListener(v -> {
            if (inputOverlaySurfaceView.getInputMode() == InputOverlaySurfaceView.InputMode.EDIT_SIZE)
                return;
            binding.moveInputsButton.setAlpha(0.5f);
            binding.resizeInputsButton.setAlpha(1.0f);
            toastMessage(R.string.input_mode_edit_size);
            inputOverlaySurfaceView.setInputMode(InputOverlaySurfaceView.InputMode.EDIT_SIZE);
        });
        binding.finishEditInputsButton.setOnClickListener(v -> {
            inputOverlaySurfaceView.setInputMode(InputOverlaySurfaceView.InputMode.DEFAULT);
            binding.finishEditInputsButton.setVisibility(View.GONE);
            binding.editInputsLayout.setVisibility(View.GONE);
            toastMessage(R.string.input_mode_default);
        });
        settingsMenu = new PopupMenu(requireContext(), binding.emulationSettingsButton);
        settingsMenu.getMenuInflater().inflate(R.menu.menu_emulation_in_game, settingsMenu.getMenu());
        settingsMenu.setOnMenuItemClickListener(EmulationFragment.this);
        binding.emulationSettingsButton.setOnClickListener(v -> settingsMenu.show());

        if (!overlaySettings.isOverlayEnabled()) {
            inputOverlaySurfaceView.setVisibility(View.GONE);
        }
        SurfaceView mainCanvas = binding.mainCanvas;

        NativeLibrary.initializerRenderer(testSurface);

        var mainCanvasHolder = mainCanvas.getHolder();
        mainCanvasHolder.addCallback(new SurfaceHolderCallback(true));
        mainCanvasHolder.addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceCreated(@NonNull SurfaceHolder holder) {
            }

            @Override
            public void surfaceChanged(@NonNull SurfaceHolder holder, int format, int width, int height) {
                if (!isGameRunning) {
                    isGameRunning = true;
                    NativeLibrary.startGame(gameTitleId);
                }
            }

            @Override
            public void surfaceDestroyed(@NonNull SurfaceHolder holder) {
            }
        });
        mainCanvas.setOnTouchListener(new OnSurfaceTouchListener(true));
        return binding.getRoot();
    }
}