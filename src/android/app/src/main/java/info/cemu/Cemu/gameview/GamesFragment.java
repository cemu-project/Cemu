package info.cemu.Cemu.gameview;

import android.content.Intent;
import android.os.Bundle;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.lifecycle.ViewModelProvider;
import androidx.recyclerview.widget.RecyclerView;

import info.cemu.Cemu.databinding.FragmentGamesBinding;
import info.cemu.Cemu.emulation.EmulationActivity;
import info.cemu.Cemu.settings.SettingsActivity;

public class GamesFragment extends Fragment {
    private GameAdapter gameAdapter;
    private GameViewModel gameViewModel;

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        gameAdapter = new GameAdapter(titleId -> {
            Intent intent = new Intent(getContext(), EmulationActivity.class);
            intent.putExtra(EmulationActivity.GAME_TITLE_ID, titleId);
            startActivity(intent);
        });
        gameViewModel = new ViewModelProvider(this).get(GameViewModel.class);
        gameViewModel.getGames().observe(this, gameList -> gameAdapter.submitList(gameList));
    }

    @Override
    public void onResume() {
        super.onResume();
        gameViewModel.refreshGames();
    }

    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        FragmentGamesBinding binding = FragmentGamesBinding.inflate(inflater, container, false);
        RecyclerView recyclerView = binding.gamesRecyclerView;
        binding.settingsButton.setOnClickListener(v -> {
            Intent intent = new Intent(requireActivity(), SettingsActivity.class);
            startActivity(intent);
        });
        View rootView = binding.getRoot();

        binding.gamesSwipeRefresh.setOnRefreshListener(() -> {
            gameViewModel.refreshGames();
            binding.gamesSwipeRefresh.setRefreshing(false);
        });
        recyclerView.setAdapter(gameAdapter);
        binding.searchText.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence charSequence, int i, int i1, int i2) {
            }

            @Override
            public void onTextChanged(CharSequence charSequence, int i, int i1, int i2) {
                gameAdapter.setFilterText(charSequence.toString());
            }

            @Override
            public void afterTextChanged(Editable editable) {
            }
        });

        return rootView;
    }

}