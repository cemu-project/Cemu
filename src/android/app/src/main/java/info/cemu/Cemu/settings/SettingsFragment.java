package info.cemu.Cemu.settings;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;
import androidx.navigation.fragment.NavHostFragment;

import info.cemu.Cemu.R;
import info.cemu.Cemu.databinding.GenericRecyclerViewLayoutBinding;
import info.cemu.Cemu.guibasecomponents.ButtonRecyclerViewItem;
import info.cemu.Cemu.guibasecomponents.GenericRecyclerViewAdapter;
import info.cemu.Cemu.guibasecomponents.SimpleButtonRecyclerViewItem;

public class SettingsFragment extends Fragment {
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        GenericRecyclerViewLayoutBinding binding = GenericRecyclerViewLayoutBinding.inflate(inflater, container, false);
        GenericRecyclerViewAdapter genericRecyclerViewAdapter = new GenericRecyclerViewAdapter();

        genericRecyclerViewAdapter.addRecyclerViewItem(new ButtonRecyclerViewItem(getString(R.string.add_game_path), getString(R.string.games_folder_description), () -> NavHostFragment.findNavController(SettingsFragment.this).navigate(R.id.action_settingsFragment_to_gamePathsFragment)));
        genericRecyclerViewAdapter.addRecyclerViewItem(new SimpleButtonRecyclerViewItem(getString(R.string.input_settings), () -> NavHostFragment.findNavController(SettingsFragment.this).navigate(R.id.action_settingsFragment_to_inputSettingsFragment)));
        genericRecyclerViewAdapter.addRecyclerViewItem(new SimpleButtonRecyclerViewItem(getString(R.string.graphics_settings), () -> NavHostFragment.findNavController(SettingsFragment.this).navigate(R.id.action_settingsFragment_to_graphicSettingsFragment)));
        genericRecyclerViewAdapter.addRecyclerViewItem(new SimpleButtonRecyclerViewItem(getString(R.string.audio_settings), () -> NavHostFragment.findNavController(SettingsFragment.this).navigate(R.id.action_settingsFragment_to_audioSettingsFragment)));
        genericRecyclerViewAdapter.addRecyclerViewItem(new SimpleButtonRecyclerViewItem(getString(R.string.graphic_packs), () -> NavHostFragment.findNavController(SettingsFragment.this).navigate(R.id.action_settingsFragment_to_graphicPacksRootFragment)));
        genericRecyclerViewAdapter.addRecyclerViewItem(new SimpleButtonRecyclerViewItem(getString(R.string.overlay), () -> NavHostFragment.findNavController(SettingsFragment.this).navigate(R.id.action_settingsFragment_to_overlaySettingsFragment)));
        binding.recyclerView.setAdapter(genericRecyclerViewAdapter);

        return binding.getRoot();
    }
}