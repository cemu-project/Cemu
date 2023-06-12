package info.cemu.Cemu;

import android.content.Intent;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import info.cemu.Cemu.databinding.FragmentGamesBinding;

public class GamesFragment extends Fragment {
    FragmentGamesBinding binding;
    GameAdapter gameAdapter;

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
    }

    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        binding = FragmentGamesBinding.inflate(inflater, container, false);
        RecyclerView recyclerView = binding.gamesRecyclerView;
        recyclerView.setLayoutManager(new LinearLayoutManager(getContext()));
        View rootView = binding.getRoot();
        gameAdapter = new GameAdapter(titleId -> {
            Intent intent = new Intent(getContext(), EmulationActivity.class);
            intent.putExtra(EmulationActivity.GAME_TITLE_ID, titleId);
            startActivity(intent);
        });
        binding.gamesSwipeRefresh.setOnRefreshListener(() -> {
            gameAdapter.clear();
            NativeLibrary.reloadGameTitles();
            binding.gamesSwipeRefresh.setRefreshing(false);
        });
        recyclerView.setAdapter(gameAdapter);
        NativeLibrary.setGameTitleLoadedCallback((titleId, title) -> {
            requireActivity().runOnUiThread(() -> {
                gameAdapter.addGameInfo(titleId, title);
                NativeLibrary.requestGameIcon(titleId);
            });
        });
        NativeLibrary.setGameIconLoadedCallback((titleId, colors, width, height) -> {
            requireActivity().runOnUiThread(() -> gameAdapter.setGameIcon(titleId, colors, width, height));
        });
        NativeLibrary.reloadGameTitles();
        return rootView;
    }

}