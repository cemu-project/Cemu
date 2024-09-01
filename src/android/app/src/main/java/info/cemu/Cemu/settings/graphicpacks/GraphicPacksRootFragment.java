package info.cemu.Cemu.settings.graphicpacks;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.widget.SearchView;
import androidx.core.view.MenuProvider;
import androidx.fragment.app.Fragment;
import androidx.lifecycle.Lifecycle;
import androidx.lifecycle.ViewModel;
import androidx.lifecycle.ViewModelProvider;
import androidx.navigation.fragment.NavHostFragment;

import com.google.android.material.dialog.MaterialAlertDialogBuilder;
import com.google.android.material.snackbar.Snackbar;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.List;
import java.util.Objects;
import java.util.Optional;

import info.cemu.Cemu.R;
import info.cemu.Cemu.databinding.GenericRecyclerViewLayoutBinding;
import info.cemu.Cemu.guibasecomponents.FilterableRecyclerViewAdapter;
import info.cemu.Cemu.guibasecomponents.GenericRecyclerViewAdapter;
import info.cemu.Cemu.NativeLibrary;
import info.cemu.Cemu.utils.FileUtil;
import info.cemu.Cemu.utils.ZipUtil;
import okhttp3.Call;
import okhttp3.Callback;
import okhttp3.OkHttpClient;
import okhttp3.Request;
import okhttp3.Response;
import okhttp3.ResponseBody;

public class GraphicPacksRootFragment extends Fragment {
    public abstract static class GraphicPackNode {
        protected boolean titleIdInstalled;
        protected String name;

        public boolean hasTitleIdInstalled() {
            return titleIdInstalled;
        }

        public String getName() {
            return name;
        }
    }

    public static class GraphicPackDataNode extends GraphicPackNode {
        private final long id;
        private final String path;

        public GraphicPackDataNode(long id, String name, String path, boolean hasTitleIdInstalled) {
            this.id = id;
            this.name = name;
            this.path = path;
            this.titleIdInstalled = hasTitleIdInstalled;
        }

        public long getId() {
            return id;
        }

        public String getPath() {
            return path;
        }
    }

    public static class GraphicPackSectionNode extends GraphicPackNode {
        ArrayList<GraphicPackNode> children = new ArrayList<>();

        public GraphicPackSectionNode(String name, boolean hasTitleIdInstalled) {
            this.name = name;
            this.titleIdInstalled = hasTitleIdInstalled;
        }

        public GraphicPackSectionNode() {
        }

        void clear() {
            children.clear();
        }

        private void addGraphicPackDataByTokens(List<String> tokens, GraphicPackDataNode dataNode) {
            var node = this;
            for (var token : tokens) {
                node = getOrAddSectionByToken(token, node.children, dataNode.titleIdInstalled);
            }
            node.children.add(dataNode);
        }

        private GraphicPackSectionNode getOrAddSectionByToken(String token, ArrayList<GraphicPackNode> graphicPackNodes, boolean hasTitleIdInstalled) {
            var sectionNodeOptional = graphicPackNodes.stream()
                    .filter(g -> (g instanceof GraphicPackSectionNode) && g.name.equals(token))
                    .findFirst();
            if (!sectionNodeOptional.isPresent()) {
                var sectionNode = new GraphicPackSectionNode(token, hasTitleIdInstalled);
                graphicPackNodes.add(sectionNode);
                return sectionNode;
            }
            return (GraphicPackSectionNode) sectionNodeOptional.get();
        }

        private void sort() {
            children.forEach(node -> {
                if (node instanceof GraphicPackSectionNode sectionNode) sectionNode.sort();
            });
            children.sort(Comparator.comparing(o -> o.name));
        }
    }

    public static class GraphicPackViewModel extends ViewModel {
        private GraphicPackNode graphicPackNode;

        public GraphicPackViewModel() {
        }

        public GraphicPackNode getGraphicPackNode() {
            return graphicPackNode;
        }

        public void setGraphicPackNode(GraphicPackNode graphicPackNode) {
            this.graphicPackNode = graphicPackNode;
        }
    }

    private final ArrayList<Long> installedTitleIds = NativeLibrary.getInstalledGamesTitleIds();
    private final GraphicPackSectionNode graphicPackSectionRootNode = new GraphicPackSectionNode();
    private final FilterableRecyclerViewAdapter<GraphicPackItemRecyclerViewItem> genericRecyclerViewAdapter = new FilterableRecyclerViewAdapter<>();
    private GraphicPackViewModel graphicPackViewModel;
    private final GraphicPacksRecyclerViewAdapter graphicPacksRecyclerViewAdapter = new GraphicPacksRecyclerViewAdapter(graphicPackDataNode -> {
        graphicPackViewModel.setGraphicPackNode(graphicPackDataNode);
        Bundle bundle = new Bundle();
        bundle.putString("title", graphicPackDataNode.name);
        NavHostFragment.findNavController(GraphicPacksRootFragment.this)
                .navigate(R.id.action_graphicPacksRootFragment_to_graphicPacksFragment, bundle);
    });
    private final OkHttpClient client = new OkHttpClient();
    private static final String RELEASES_API_URL = "https://api.github.com/repos/cemu-project/cemu_graphic_packs/releases/latest";
    private boolean updateInProgress;

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        genericRecyclerViewAdapter.setPredicate(g -> g.hasInstalledTitleId);
        graphicPacksRecyclerViewAdapter.setShowInstalledOnly(true);
        fillGraphicPacks();
        var navController = NavHostFragment.findNavController(GraphicPacksRootFragment.this);
        graphicPackViewModel = new ViewModelProvider(Objects.requireNonNull(navController.getCurrentBackStackEntry()))
                .get(GraphicPackViewModel.class);
    }

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        var binding = GenericRecyclerViewLayoutBinding.inflate(inflater, container, false);
        binding.recyclerView.setAdapter(this.genericRecyclerViewAdapter);
        requireActivity().addMenuProvider(new MenuProvider() {
            @Override
            public void onCreateMenu(@NonNull Menu menu, @NonNull MenuInflater menuInflater) {
                menuInflater.inflate(R.menu.menu_graphic_packs, menu);
                var searchMenuItem = menu.findItem(R.id.action_graphic_packs_search);
                SearchView searchView = (SearchView) Objects.requireNonNull(searchMenuItem.getActionView());
                searchView.setOnQueryTextListener(new SearchView.OnQueryTextListener() {
                    @Override
                    public boolean onQueryTextSubmit(String query) {
                        graphicPacksRecyclerViewAdapter.setFilterText(query);
                        return true;
                    }

                    @Override
                    public boolean onQueryTextChange(String newText) {
                        graphicPacksRecyclerViewAdapter.setFilterText(newText);
                        return true;
                    }
                });
                searchMenuItem.setOnActionExpandListener(new MenuItem.OnActionExpandListener() {
                    @Override
                    public boolean onMenuItemActionExpand(@NonNull MenuItem item) {
                        binding.recyclerView.setAdapter(graphicPacksRecyclerViewAdapter);
                        return true;
                    }

                    @Override
                    public boolean onMenuItemActionCollapse(@NonNull MenuItem item) {
                        binding.recyclerView.setAdapter(genericRecyclerViewAdapter);
                        return true;
                    }
                });
            }

            private void showInstalledOnly(boolean installedOnly) {
                if (installedOnly) {
                    genericRecyclerViewAdapter.setPredicate(g -> g.hasInstalledTitleId);
                    return;
                }
                genericRecyclerViewAdapter.clearPredicate();
            }

            @Override
            public boolean onMenuItemSelected(@NonNull MenuItem menuItem) {
                if (menuItem.getItemId() == R.id.action_graphic_packs_download && !updateInProgress) {
                    checkForUpdates();
                    return true;
                }
                if (menuItem.getItemId() == R.id.action_graphic_packs_search) {
                    return true;
                }
                if (menuItem.getItemId() == R.id.show_installed_games_only) {
                    boolean installedOnly = !menuItem.isChecked();
                    graphicPacksRecyclerViewAdapter.setShowInstalledOnly(installedOnly);
                    menuItem.setChecked(installedOnly);
                    showInstalledOnly(installedOnly);
                    return true;
                }
                return false;
            }
        }, getViewLifecycleOwner(), Lifecycle.State.RESUMED);
        return binding.getRoot();
    }

    private void fillGraphicPacks() {
        var nativeGraphicPacks = NativeLibrary.getGraphicPackBasicInfos();
        graphicPackSectionRootNode.clear();
        List<GraphicPackDataNode> graphicPackDataNodes = new ArrayList<>();
        nativeGraphicPacks.forEach(graphicPack -> {
            var tokens = Arrays.asList(graphicPack.virtualPath().split("/"));
            boolean hasTitleIdInstalled = graphicPack.titleIds().stream().anyMatch(installedTitleIds::contains);
            var dataNode = new GraphicPackDataNode(graphicPack.id(), tokens.get(tokens.size() - 1), graphicPack.virtualPath(), hasTitleIdInstalled);
            graphicPackDataNodes.add(dataNode);
            graphicPackSectionRootNode.addGraphicPackDataByTokens(tokens.subList(0, tokens.size() - 1), dataNode);
        });
        graphicPacksRecyclerViewAdapter.setItems(graphicPackDataNodes);

        genericRecyclerViewAdapter.clearRecyclerViewItems();
        graphicPackSectionRootNode.sort();
        graphicPackSectionRootNode.children.forEach(node -> {
            var graphicPackItemType = node instanceof GraphicPackSectionNode
                    ? GraphicPackItemRecyclerViewItem.GraphicPackItemType.SECTION
                    : GraphicPackItemRecyclerViewItem.GraphicPackItemType.GRAPHIC_PACK;
            GraphicPackItemRecyclerViewItem graphicPackItemRecyclerViewItem = new GraphicPackItemRecyclerViewItem(node.name,
                    node.hasTitleIdInstalled(),
                    graphicPackItemType,
                    () -> {
                        graphicPackViewModel.setGraphicPackNode(node);
                        Bundle bundle = new Bundle();
                        bundle.putString("title", node.name);
                        NavHostFragment.findNavController(GraphicPacksRootFragment.this)
                                .navigate(R.id.action_graphicPacksRootFragment_to_graphicPacksFragment, bundle);
                    });
            genericRecyclerViewAdapter.addRecyclerViewItem(graphicPackItemRecyclerViewItem);
        });
    }

    private void checkForUpdates() {
        AlertDialog dialog = createGraphicPacksDownloadDialog();
        requireActivity().runOnUiThread(() -> Objects.requireNonNull((TextView) dialog.findViewById(R.id.textViewGraphicPacksDownload)).setText(R.string.checking_version_download_text));
        Request request = new Request.Builder()
                .url(RELEASES_API_URL)
                .build();
        requireActivity().runOnUiThread(() -> Objects.requireNonNull((TextView) dialog.findViewById(R.id.textViewGraphicPacksDownload)).setText(R.string.graphic_packs_download_text));
        client.newCall(request).enqueue(new Callback() {
            @Override
            public void onResponse(@NonNull Call call, @NonNull Response response) throws IOException {
                try (ResponseBody responseBody = response.body()) {
                    var json = new JSONObject(Objects.requireNonNull(responseBody).string());
                    var version = json.getString("name");
                    var currentVersion = getCurrentVersion();
                    if (currentVersion.isPresent() && currentVersion.get().equals(version)) {
                        onGraphicPacksLatestVersion(dialog);
                        return;
                    }
                    var downloadUrl = json.getJSONArray("assets")
                            .getJSONObject(0)
                            .getString("browser_download_url");
                    downloadNewUpdate(downloadUrl, version, dialog);
                } catch (JSONException e) {
                    requireActivity().runOnUiThread(dialog::dismiss);
                }
            }

            @Override
            public void onFailure(@NonNull Call call, @NonNull IOException e) {
                if (!call.isCanceled()) onDownloadError(dialog);
            }
        });
    }

    private Optional<String> getCurrentVersion() {
        Path graphicPacksVersionFile = Paths.get(Objects.requireNonNull(requireActivity().getExternalFilesDir(null)).getAbsolutePath(),
                "graphicPacks",
                "downloadedGraphicPacks",
                "version.txt");
        try {
            return Optional.of(new String(Files.readAllBytes(graphicPacksVersionFile)));
        } catch (IOException ignored) {
            return Optional.empty();
        }
    }

    private void downloadNewUpdate(String graphicPacksZipDownloadUrl, String version, @NonNull AlertDialog dialog) {
        Request request = new Request.Builder()
                .url(graphicPacksZipDownloadUrl)
                .build();

        Path graphicPacksDirPath = Paths.get(Objects.requireNonNull(requireActivity().getExternalFilesDir(null)).getAbsolutePath(), "graphicPacks");
        client.newCall(request).enqueue(new Callback() {
            @Override
            public void onResponse(@NonNull Call call, @NonNull Response response) throws IOException {
                try (ResponseBody responseBody = response.body()) {
                    Path graphicPacksTempDirPath = graphicPacksDirPath.resolve("downloadedGraphicPacksTemp");
                    FileUtil.delete(graphicPacksTempDirPath);
                    ZipUtil.unzip(Objects.requireNonNull(responseBody).byteStream(), graphicPacksTempDirPath.toString());
                    Files.write(graphicPacksTempDirPath.resolve("version.txt"), version.getBytes(StandardCharsets.UTF_8));
                    Path downloadedGraphicPacksDirPath = graphicPacksDirPath.resolve("downloadedGraphicPacks");
                    FileUtil.delete(downloadedGraphicPacksDirPath);
                    Files.move(graphicPacksTempDirPath, downloadedGraphicPacksDirPath);
                    NativeLibrary.refreshGraphicPacks();
                    requireActivity().runOnUiThread(() -> fillGraphicPacks());
                    onDownloadFinish(dialog);
                }
            }

            @Override
            public void onFailure(@NonNull Call call, @NonNull IOException e) {
                if (!call.isCanceled()) onDownloadError(dialog);
            }
        });
    }

    private void onDownloadError(AlertDialog dialog) {
        requireActivity().runOnUiThread(() -> {
            dialog.dismiss();
            Snackbar.make(requireView(), R.string.download_graphic_packs_error_text, Snackbar.LENGTH_SHORT).show();
        });
    }

    private void onDownloadFinish(AlertDialog dialog) {
        requireActivity().runOnUiThread(() -> {
            dialog.dismiss();
            Snackbar.make(requireView(), R.string.download_graphic_packs_finish_text, Snackbar.LENGTH_SHORT).show();
        });
    }

    private void onGraphicPacksLatestVersion(AlertDialog dialog) {
        requireActivity().runOnUiThread(() -> {
            dialog.dismiss();
            Snackbar.make(requireView(), R.string.graphic_packs_no_updates_text, Snackbar.LENGTH_SHORT).show();
        });
    }

    private AlertDialog createGraphicPacksDownloadDialog() {
        updateInProgress = true;
        return new MaterialAlertDialogBuilder(requireContext()).setView(R.layout.layout_graphic_packs_download)
                .setTitle(R.string.graphic_packs_download_dialog_title)
                .setCancelable(false)
                .setOnDismissListener(d -> updateInProgress = false)
                .setNegativeButton(R.string.cancel, (dialogInterface, i) -> client.dispatcher().cancelAll())
                .show();
    }

    @Override
    public void onDestroy() {
        client.dispatcher().cancelAll();
        super.onDestroy();
    }
}