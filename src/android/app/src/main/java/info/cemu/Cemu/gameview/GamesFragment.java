package info.cemu.Cemu.gameview;

import android.content.Intent;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import info.cemu.Cemu.R;
import info.cemu.Cemu.databinding.FragmentGamesBinding;
import info.cemu.Cemu.emulation.EmulationActivity;
import info.cemu.Cemu.NativeLibrary;
import info.cemu.Cemu.settings.SettingsActivity;
import info.cemu.Cemu.settings.SettingsManager;

public class GamesFragment extends Fragment {
    private FragmentGamesBinding binding;
    private GameAdapter gameAdapter;
    private SettingsManager settingsManager;

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        settingsManager = new SettingsManager(requireContext());
    }

    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        binding = FragmentGamesBinding.inflate(inflater, container, false);
        String gamesPath = settingsManager.getGamesPath();
        if (gamesPath != null)
            NativeLibrary.addGamePath(gamesPath);
        RecyclerView recyclerView = binding.gamesRecyclerView;
        recyclerView.setLayoutManager(new LinearLayoutManager(getContext()));
        binding.settingsButton.setOnClickListener(v -> {
            Intent intent = new Intent(requireActivity(), SettingsActivity.class);
            startActivity(intent);
        });
        View rootView = binding.getRoot();
        gameAdapter = new GameAdapter(titleId -> {
            Intent intent = new Intent(getContext(), EmulationActivity.class);
            intent.putExtra(EmulationActivity.GAME_TITLE_ID, titleId);
            startActivity(intent);
        });
        gameAdapter.registerAdapterDataObserver(new RecyclerView.AdapterDataObserver() {
            @Override
            public void onChanged() {
                if (gameAdapter.getItemCount() == 0) {
                    if (!binding.searchText.getText().toString().isEmpty())
                        binding.gamesListMessage.setText(R.string.no_games_found);
                    binding.gamesListMessage.setVisibility(View.VISIBLE);

                } else {
                    binding.gamesListMessage.setVisibility(View.GONE);
                }
            }
        });
        binding.gamesSwipeRefresh.setOnRefreshListener(() -> {
            gameAdapter.clear();
            NativeLibrary.reloadGameTitles();
            binding.gamesSwipeRefresh.setRefreshing(false);
        });
        recyclerView.setAdapter(gameAdapter);
        NativeLibrary.setGameTitleLoadedCallback((titleId, title) -> requireActivity().runOnUiThread(() -> {
            gameAdapter.addGameInfo(titleId, title);
            NativeLibrary.requestGameIcon(titleId);
        }));
        NativeLibrary.setGameIconLoadedCallback((titleId, colors, width, height) -> {
            Bitmap icon = Bitmap.createBitmap(colors, width, height, Bitmap.Config.ARGB_8888);
            requireActivity().runOnUiThread(() -> gameAdapter.setGameIcon(titleId, icon));
        });
        NativeLibrary.reloadGameTitles();

        binding.searchText.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence charSequence, int i, int i1, int i2) {

            }

            @Override
            public void onTextChanged(CharSequence charSequence, int i, int i1, int i2) {
                gameAdapter.getFilter().filter(charSequence.toString());
            }

            @Override
            public void afterTextChanged(Editable editable) {

            }
        });

        return rootView;
    }

}