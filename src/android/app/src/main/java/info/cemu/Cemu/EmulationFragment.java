package info.cemu.Cemu;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;

import info.cemu.Cemu.databinding.FragmentEmulationBinding;

public class EmulationFragment extends Fragment implements SurfaceHolder.Callback {
    FragmentEmulationBinding binding;

    public EmulationFragment() {
    }

    static EmulationFragment newInstance(long gameTitleId) {
        EmulationFragment emulationFragment = new EmulationFragment();
        Bundle arguments = new Bundle();
        arguments.putLong(EmulationActivity.GAME_TITLE_ID, gameTitleId);
        emulationFragment.setArguments(arguments);
        return emulationFragment;
    }

    long gameTitleId;
    boolean isGameRunning;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Bundle arguments = getArguments();
        if (arguments != null) {
            gameTitleId = arguments.getLong(EmulationActivity.GAME_TITLE_ID);
        }

    }

    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        binding = FragmentEmulationBinding.inflate(inflater, container, false);
        SurfaceView mainCanvas = binding.mainCanvas;
        mainCanvas.getHolder().addCallback(this);
        return binding.getRoot();
    }

    @Override
    public void surfaceCreated(@NonNull SurfaceHolder surfaceHolder) {

    }

    @Override
    public void surfaceChanged(@NonNull SurfaceHolder surfaceHolder, int format, int width, int height) {
        NativeLibrary.setSurface(surfaceHolder.getSurface(), true);
        NativeLibrary.setSurfaceSize(width, height, true);
        if (!isGameRunning) {
            isGameRunning = true;
            NativeLibrary.initializerRenderer();
            NativeLibrary.initializeRendererSurface(true);
            NativeLibrary.startGame(gameTitleId);
        }else{
            NativeLibrary.recreateRenderSurface(true);
        }
    }

    @Override
    public void surfaceDestroyed(@NonNull SurfaceHolder surfaceHolder) {
        NativeLibrary.clearSurface(true);
    }
}