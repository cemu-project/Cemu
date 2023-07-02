package info.cemu.Cemu;

import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;

import info.cemu.Cemu.databinding.FragmentEmulationBinding;

public class EmulationFragment extends Fragment implements SurfaceHolder.Callback {
    private final long gameTitleId;
    private boolean isGameRunning;
    private SurfaceHolder mainCanvasSurfaceHolder;

    public EmulationFragment(long gameTitleId) {
        this.gameTitleId = gameTitleId;
    }

    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        FragmentEmulationBinding binding = FragmentEmulationBinding.inflate(inflater, container, false);
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