package info.cemu.Cemu.settings.graphics;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.StringRes;
import androidx.fragment.app.Fragment;

import java.util.stream.Collectors;
import java.util.stream.Stream;

import info.cemu.Cemu.R;
import info.cemu.Cemu.databinding.GenericRecyclerViewLayoutBinding;
import info.cemu.Cemu.guibasecomponents.CheckboxRecyclerViewItem;
import info.cemu.Cemu.guibasecomponents.GenericRecyclerViewAdapter;
import info.cemu.Cemu.guibasecomponents.SelectionAdapter;
import info.cemu.Cemu.guibasecomponents.SingleSelectionRecyclerViewItem;
import info.cemu.Cemu.NativeLibrary;

public class GraphicsSettingsFragment extends Fragment {
    private static int vsyncModeToResourceNameId(int vsyncMode) {
        return switch (vsyncMode) {
            case NativeLibrary.VSYNC_MODE_OFF -> R.string.off;
            case NativeLibrary.VSYNC_MODE_DOUBLE_BUFFERING -> R.string.double_buffering;
            case NativeLibrary.VSYNC_MODE_TRIPLE_BUFFERING -> R.string.triple_buffering;
            default -> throw new IllegalArgumentException("Invalid vsync mode: " + vsyncMode);
        };
    }

    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        var binding = GenericRecyclerViewLayoutBinding.inflate(inflater, container, false);

        GenericRecyclerViewAdapter genericRecyclerViewAdapter = new GenericRecyclerViewAdapter();

        CheckboxRecyclerViewItem asyncShaderCheckbox = new CheckboxRecyclerViewItem(getString(R.string.async_shader_compile), getString(R.string.async_shader_compile_description), NativeLibrary.getAsyncShaderCompile(), NativeLibrary::setAsyncShaderCompile);
        genericRecyclerViewAdapter.addRecyclerViewItem(asyncShaderCheckbox);

        int vsyncMode = NativeLibrary.getVSyncMode();
        var vsyncChoices = Stream.of(NativeLibrary.VSYNC_MODE_OFF, NativeLibrary.VSYNC_MODE_DOUBLE_BUFFERING, NativeLibrary.VSYNC_MODE_TRIPLE_BUFFERING)
                .map(vsync -> new SelectionAdapter.ChoiceItem<>(t -> t.setText(vsyncModeToResourceNameId(vsync)), vsync))
                .collect(Collectors.toList());
        SelectionAdapter<Integer> vsyncSelectionAdapter = new SelectionAdapter<>(vsyncChoices, vsyncMode);
        SingleSelectionRecyclerViewItem<Integer> vsyncModeSelection = new SingleSelectionRecyclerViewItem<>(getString(R.string.vsync),
                getString(vsyncModeToResourceNameId(vsyncMode)), vsyncSelectionAdapter,
                (vsync, selectionRecyclerViewItem) -> {
                    NativeLibrary.setVSyncMode(vsync);
                    selectionRecyclerViewItem.setDescription(getString(vsyncModeToResourceNameId(vsync)));
                });
        genericRecyclerViewAdapter.addRecyclerViewItem(vsyncModeSelection);

        CheckboxRecyclerViewItem accurateBarriersCheckbox = new CheckboxRecyclerViewItem(getString(R.string.accurate_barriers), getString(R.string.accurate_barriers_description), NativeLibrary.getAccurateBarriers(), NativeLibrary::setAccurateBarriers);
        genericRecyclerViewAdapter.addRecyclerViewItem(accurateBarriersCheckbox);

        binding.recyclerView.setAdapter(genericRecyclerViewAdapter);
        return binding.getRoot();
    }
}