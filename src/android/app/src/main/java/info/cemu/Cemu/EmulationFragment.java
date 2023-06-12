package info.cemu.Cemu;

import android.content.Intent;
import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import android.view.LayoutInflater;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;

import java.util.Objects;

import info.cemu.Cemu.databinding.FragmentEmulationBinding;
import info.cemu.Cemu.databinding.FragmentGamesBinding;

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
    boolean isRunning;

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

    private void startGame() {
        if (!isRunning) {
            isRunning = true;
            NativeLibrary.initializerRenderer();
            NativeLibrary.initializeRendererSurface(true);
            NativeLibrary.startGame(gameTitleId);
        }
    }

    @Override
    public void surfaceCreated(@NonNull SurfaceHolder surfaceHolder) {

    }

    @Override
    public void surfaceChanged(@NonNull SurfaceHolder surfaceHolder, int format, int width, int height) {
        NativeLibrary.setSurface(surfaceHolder.getSurface(), true);
        NativeLibrary.setSurfaceSize(width, height, true);
        startGame();
    }

    @Override
    public void surfaceDestroyed(@NonNull SurfaceHolder surfaceHolder) {

    }
}