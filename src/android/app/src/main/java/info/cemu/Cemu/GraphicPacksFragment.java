package info.cemu.Cemu;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.lifecycle.ViewModelProvider;
import androidx.navigation.NavController;
import androidx.navigation.fragment.NavHostFragment;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Objects;
import java.util.function.Consumer;

import info.cemu.Cemu.databinding.FragmentGraphicPacksBinding;

public class GraphicPacksFragment extends Fragment {
    private final GenericRecyclerViewAdapter genericRecyclerViewAdapter = new GenericRecyclerViewAdapter();
    private GraphicPacksRootFragment.GraphicPackViewModel graphicPackPreviousViewModel;
    private GraphicPacksRootFragment.GraphicPackViewModel graphicPackCurrentViewModel;

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        NavController navController = NavHostFragment.findNavController(this);
        graphicPackPreviousViewModel = new ViewModelProvider(Objects.requireNonNull(navController.getPreviousBackStackEntry()))
                .get(GraphicPacksRootFragment.GraphicPackViewModel.class);
        graphicPackCurrentViewModel = new ViewModelProvider(Objects.requireNonNull(navController.getCurrentBackStackEntry()))
                .get(GraphicPacksRootFragment.GraphicPackViewModel.class);
        fillData();
    }

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        FragmentGraphicPacksBinding binding = FragmentGraphicPacksBinding.inflate(inflater, container, false);
        binding.graphicPacksRecyclerView.setAdapter(genericRecyclerViewAdapter);

        return binding.getRoot();
    }

    private void fillData() {
        var graphicNode = graphicPackPreviousViewModel.getGraphicPackNode();
        if (graphicNode instanceof GraphicPacksRootFragment.GraphicPackSectionNode sectionNode) {
            fillData(sectionNode);
        } else if (graphicNode instanceof GraphicPacksRootFragment.GraphicPackDataNode dataNode) {
            fillData(dataNode);
        }
    }

    private void fillData(GraphicPacksRootFragment.GraphicPackSectionNode graphicPackSectionNode) {
        graphicPackSectionNode.children.forEach(node -> {
            var graphicPackItemType = node instanceof GraphicPacksRootFragment.GraphicPackSectionNode
                    ? GraphicPackItemRecyclerViewItem.GraphicPackItemType.SECTION
                    : GraphicPackItemRecyclerViewItem.GraphicPackItemType.GRAPHIC_PACK;
            GraphicPackItemRecyclerViewItem graphicPackItemRecyclerViewItem = new GraphicPackItemRecyclerViewItem(node.name,
                    graphicPackItemType,
                    () -> {
                        graphicPackCurrentViewModel.setGraphicPackNode(node);
                        Bundle bundle = new Bundle();
                        bundle.putString("title", node.name);
                        NavHostFragment.findNavController(GraphicPacksFragment.this)
                                .navigate(R.id.action_graphicPacksFragment_self, bundle);
                    });
            genericRecyclerViewAdapter.addRecyclerViewItem(graphicPackItemRecyclerViewItem);
        });
    }

    private void fillData(GraphicPacksRootFragment.GraphicPackDataNode graphicPackDataNode) {
        genericRecyclerViewAdapter.clearRecyclerViewItems();
        var graphicPack = NativeLibrary.getGraphicPack(graphicPackDataNode.getId());
        genericRecyclerViewAdapter.addRecyclerViewItem(new GraphicPackRecyclerViewItem(graphicPack));
        fillPresets(graphicPack);
    }

    private void fillPresets(NativeLibrary.GraphicPack graphicPack) {
        var recyclerViewItems = new ArrayList<SingleSelectionRecyclerViewItem<String>>();
        graphicPack.presets.forEach(graphicPackPreset -> {
            String category = graphicPackPreset.getCategory();
            if (category == null) category = getString(R.string.active_preset_category);
            var recyclerViewItem = new SingleSelectionRecyclerViewItem<>(
                    category,
                    graphicPackPreset.getActivePreset(),
                    new StringSelectionAdapter(
                            graphicPackPreset.getPresets(),
                            graphicPackPreset.getActivePreset()
                    ),
                    (selectedValue, selectionRecyclerViewItem) -> {
                        graphicPackPreset.setActivePreset(selectedValue);
                        selectionRecyclerViewItem.setDescription(selectedValue);
                        var oldPresets = graphicPack.presets;
                        graphicPack.reloadPresets();
                        if (!new HashSet<>(oldPresets).containsAll(graphicPack.presets)) {
                            recyclerViewItems.forEach(genericRecyclerViewAdapter::removeRecyclerViewItem);
                            fillPresets(graphicPack);
                        }
                    }
            );
            recyclerViewItems.add(recyclerViewItem);
            genericRecyclerViewAdapter.addRecyclerViewItem(recyclerViewItem);
        });
    }
}