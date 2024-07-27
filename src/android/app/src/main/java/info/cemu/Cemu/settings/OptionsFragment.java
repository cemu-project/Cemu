package info.cemu.Cemu.settings;

import static android.app.Activity.RESULT_OK;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.documentfile.provider.DocumentFile;
import androidx.fragment.app.Fragment;
import androidx.navigation.fragment.NavHostFragment;

import java.util.Objects;

import info.cemu.Cemu.R;
import info.cemu.Cemu.databinding.FragmentOptionsBinding;
import info.cemu.Cemu.guibasecomponents.ButtonRecyclerViewItem;
import info.cemu.Cemu.guibasecomponents.GenericRecyclerViewAdapter;
import info.cemu.Cemu.guibasecomponents.SimpleButtonRecyclerViewItem;

public class OptionsFragment extends Fragment {
    private ActivityResultLauncher<Intent> folderSelectionLauncher;
    private SettingsManager settingsManager;

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        settingsManager = new SettingsManager(requireContext());
    }

    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        FragmentOptionsBinding binding = FragmentOptionsBinding.inflate(inflater, container, false);
        GenericRecyclerViewAdapter genericRecyclerViewAdapter = new GenericRecyclerViewAdapter();
        folderSelectionLauncher = registerForActivityResult(new ActivityResultContracts.StartActivityForResult(), result -> {
            if (result.getResultCode() == RESULT_OK) {
                Intent data = result.getData();
                if (data != null) {
                    Uri uri = Objects.requireNonNull(data.getData());
                    requireActivity().getContentResolver().takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
                    DocumentFile documentFile = DocumentFile.fromTreeUri(requireContext(), uri);
                    if (documentFile == null) return;
                    String gamesPath = documentFile.getUri().toString();
                    settingsManager.setGamesPath(gamesPath);
                }
            }
        });

        genericRecyclerViewAdapter.addRecyclerViewItem(new ButtonRecyclerViewItem(getString(R.string.games_folder_label), getString(R.string.games_folder_description), () -> {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
            folderSelectionLauncher.launch(intent);
        }));
        genericRecyclerViewAdapter.addRecyclerViewItem(new SimpleButtonRecyclerViewItem(getString(R.string.input_settings), () -> NavHostFragment.findNavController(OptionsFragment.this).navigate(R.id.action_optionsFragment_to_inputSettingsFragment)));
        genericRecyclerViewAdapter.addRecyclerViewItem(new SimpleButtonRecyclerViewItem(getString(R.string.graphics_settings), () -> NavHostFragment.findNavController(OptionsFragment.this).navigate(R.id.action_optionsFragment_to_graphicSettingsFragment)));
        genericRecyclerViewAdapter.addRecyclerViewItem(new SimpleButtonRecyclerViewItem(getString(R.string.audio_settings), () -> NavHostFragment.findNavController(OptionsFragment.this).navigate(R.id.action_optionsFragment_to_audioSettingsFragment)));
        genericRecyclerViewAdapter.addRecyclerViewItem(new SimpleButtonRecyclerViewItem(getString(R.string.graphic_packs), () -> NavHostFragment.findNavController(OptionsFragment.this).navigate(R.id.action_optionsFragment_to_graphicPacksRootFragment)));
        genericRecyclerViewAdapter.addRecyclerViewItem(new SimpleButtonRecyclerViewItem(getString(R.string.overlay), () -> NavHostFragment.findNavController(OptionsFragment.this).navigate(R.id.action_optionsFragment_to_overlaySettingsFragment)));
        binding.optionsRecyclerView.setAdapter(genericRecyclerViewAdapter);

        return binding.getRoot();
    }
}