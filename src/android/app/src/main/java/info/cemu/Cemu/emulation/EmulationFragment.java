package info.cemu.Cemu.emulation;

import android.content.Context;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

import info.cemu.Cemu.R;
import info.cemu.Cemu.databinding.FragmentEmulationBinding;
import info.cemu.Cemu.inputoverlay.InputOverlaySettingsProvider;
import info.cemu.Cemu.inputoverlay.InputOverlaySurfaceView;
import info.cemu.Cemu.NativeLibrary;

public class EmulationFragment extends Fragment implements SurfaceHolder.Callback {
    private final long gameTitleId;
    private boolean isGameRunning;
    private SurfaceHolder mainCanvasSurfaceHolder;
    private Toast toast;

    public EmulationFragment(long gameTitleId) {
        this.gameTitleId = gameTitleId;
    }

    InputOverlaySettingsProvider.OverlaySettings overlaySettings;

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        var inputOverlaySettingsProvider = new InputOverlaySettingsProvider(requireContext());
        overlaySettings = inputOverlaySettingsProvider.getOverlaySettings();
    }

    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        FragmentEmulationBinding binding = FragmentEmulationBinding.inflate(inflater, container, false);

        InputOverlaySurfaceView surfaceView = binding.inputOverlay;
        binding.inputModeButton.setOnClickListener(v -> {
            InputOverlaySurfaceView.InputMode nextMode = InputOverlaySurfaceView.InputMode.DEFAULT;
            int toastTextResId = R.string.input_mode_default;
            switch (surfaceView.getInputMode()) {
                case EDIT_POSITION -> {
                    nextMode = InputOverlaySurfaceView.InputMode.EDIT_SIZE;
                    toastTextResId = R.string.input_mode_edit_size;
                }
                case EDIT_SIZE -> {
                    nextMode = InputOverlaySurfaceView.InputMode.DEFAULT;
                    toastTextResId = R.string.input_mode_default;
                }
                case DEFAULT -> {
                    nextMode = InputOverlaySurfaceView.InputMode.EDIT_POSITION;
                    toastTextResId = R.string.input_mode_edit_position;
                }
            }
            if (toast != null) {
                toast.cancel();
            }
            toast = Toast.makeText(requireContext(), toastTextResId, Toast.LENGTH_SHORT);
            toast.show();
            surfaceView.setInputMode(nextMode);
        });
        if (!overlaySettings.isOverlayEnabled()) {
            binding.inputModeButton.setVisibility(View.GONE);
            surfaceView.setVisibility(View.GONE);
        }
        SurfaceView mainCanvas = binding.mainCanvas;
        mainCanvas.getHolder().addCallback(this);
        return binding.getRoot();
    }

    @Override
    public void surfaceCreated(@NonNull SurfaceHolder surfaceHolder) {
        mainCanvasSurfaceHolder = surfaceHolder;
    }

    @Override
    public void surfaceChanged(@NonNull SurfaceHolder surfaceHolder, int format, int width, int height) {
        NativeLibrary.setSurfaceSize(width, height, true);
        if (!isGameRunning) {
            isGameRunning = true;
            NativeLibrary.setSurface(surfaceHolder.getSurface(), true);
            NativeLibrary.initializerRenderer();
            NativeLibrary.initializeRendererSurface(true);
            NativeLibrary.startGame(gameTitleId);
        } else {
            if (mainCanvasSurfaceHolder == surfaceHolder)
                return;
            NativeLibrary.setSurface(surfaceHolder.getSurface(), true);
            NativeLibrary.recreateRenderSurface(true);
        }
    }

    @Override
    public void surfaceDestroyed(@NonNull SurfaceHolder surfaceHolder) {
        mainCanvasSurfaceHolder = null;
        NativeLibrary.clearSurface(true);
    }
}