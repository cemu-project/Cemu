package info.cemu.Cemu;

import static android.app.Activity.RESULT_OK;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.annotation.NonNull;
import androidx.documentfile.provider.DocumentFile;
import androidx.fragment.app.Fragment;
import androidx.navigation.fragment.NavHostFragment;

import info.cemu.Cemu.databinding.FragmentOptionsBinding;

public class OptionsFragment extends Fragment {
    private ActivityResultLauncher<Intent> folderSelectionLauncher;

    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        FragmentOptionsBinding binding = FragmentOptionsBinding.inflate(inflater, container, false);
        GenericRecyclerViewAdapter genericRecyclerViewAdapter = new GenericRecyclerViewAdapter();
        folderSelectionLauncher = registerForActivityResult(new ActivityResultContracts.StartActivityForResult(), result -> {
            if (result.getResultCode() == RESULT_OK) {
                Intent data = result.getData();
                if (data != null) {
                    Uri uri = data.getData();
                    requireActivity().getContentResolver().takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
                    DocumentFile documentFile = DocumentFile.fromTreeUri(requireContext(), uri);
                    if (documentFile == null) return;
                    String gamePath = documentFile.getUri().toString();
                    NativeLibrary.addGamePath(gamePath);
                    SharedPreferences.Editor preferencesEditor = requireActivity().getSharedPreferences(CemuPreferences.PREFERENCES_NAME, Context.MODE_PRIVATE).edit();
                    preferencesEditor.putString(CemuPreferences.GAMES_PATH_KEY, gamePath);
                    preferencesEditor.apply();
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
        binding.optionsRecyclerView.setAdapter(genericRecyclerViewAdapter);

        return binding.getRoot();
    }
}