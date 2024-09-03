package info.cemu.Cemu.settings.gamespath;

import static android.app.Activity.RESULT_OK;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Toast;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.view.MenuProvider;
import androidx.documentfile.provider.DocumentFile;
import androidx.fragment.app.Fragment;
import androidx.lifecycle.Lifecycle;

import java.util.List;
import java.util.Objects;
import java.util.stream.Collectors;
import java.util.stream.Stream;

import info.cemu.Cemu.NativeLibrary;
import info.cemu.Cemu.R;
import info.cemu.Cemu.databinding.GenericRecyclerViewLayoutBinding;

public class GamePathsFragment extends Fragment {
    private ActivityResultLauncher<Intent> folderSelectionLauncher;
    private GamePathAdapter gamePathAdapter;
    private List<String> gamesPaths;

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        folderSelectionLauncher = registerForActivityResult(new ActivityResultContracts.StartActivityForResult(), result -> {
            if (result.getResultCode() == RESULT_OK) {
                Intent data = result.getData();
                if (data != null) {
                    Uri uri = Objects.requireNonNull(data.getData());
                    requireActivity().getContentResolver().takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
                    DocumentFile documentFile = DocumentFile.fromTreeUri(requireContext(), uri);
                    if (documentFile == null) return;
                    String gamesPath = documentFile.getUri().toString();
                    if (gamesPaths.stream().anyMatch(p -> p.equals(gamesPath))) {
                        Toast.makeText(requireContext(), R.string.game_path_already_added, Toast.LENGTH_LONG).show();
                        return;
                    }
                    NativeLibrary.addGamesPath(gamesPath);
                    gamesPaths = Stream.concat(Stream.of(gamesPath), gamesPaths.stream()).collect(Collectors.toList());
                    gamePathAdapter.submitList(gamesPaths);
                }
            }
        });
        gamePathAdapter = new GamePathAdapter(new GamePathAdapter.OnRemoveGamePath() {
            @Override
            public void onRemoveGamePath(String path) {
                NativeLibrary.removeGamesPath(path);
                gamesPaths = gamesPaths.stream().filter(p -> !p.equals(path)).collect(Collectors.toList());
                gamePathAdapter.submitList(gamesPaths);
            }
        });
    }

    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        GenericRecyclerViewLayoutBinding binding = GenericRecyclerViewLayoutBinding.inflate(inflater, container, false);
        binding.recyclerView.setAdapter(gamePathAdapter);
        gamesPaths = NativeLibrary.getGamesPaths();
        gamePathAdapter.submitList(gamesPaths);
        requireActivity().addMenuProvider(new MenuProvider() {
            @Override
            public void onCreateMenu(@NonNull Menu menu, @NonNull MenuInflater menuInflater) {
                menuInflater.inflate(R.menu.menu_game_paths, menu);
            }

            @Override
            public boolean onMenuItemSelected(@NonNull MenuItem menuItem) {
                if (menuItem.getItemId() == R.id.action_add_game_path) {
                    Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
                    folderSelectionLauncher.launch(intent);
                    return true;
                }
                return false;
            }
        }, getViewLifecycleOwner(), Lifecycle.State.RESUMED);
        return binding.getRoot();
    }
}