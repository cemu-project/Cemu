package info.cemu.Cemu;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;

import java.util.stream.Collectors;
import java.util.stream.Stream;

import info.cemu.Cemu.databinding.FragmentGraphicsSettingsBinding;

public class GraphicsSettingsFragment extends Fragment {

    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        FragmentGraphicsSettingsBinding binding = FragmentGraphicsSettingsBinding.inflate(inflater, container, false);

        GenericRecyclerViewAdapter genericRecyclerViewAdapter = new GenericRecyclerViewAdapter();

        CheckboxRecyclerViewItem asyncShaderCheckbox = new CheckboxRecyclerViewItem(getString(R.string.async_shader_compile), getString(R.string.async_shader_compile_description), NativeLibrary.getAsyncShaderCompile(), NativeLibrary::setAsyncShaderCompile);
        genericRecyclerViewAdapter.addRecyclerViewItem(asyncShaderCheckbox);

        int vsyncMode = NativeLibrary.getVSyncMode();
        var vsyncChoices = Stream.of(NativeLibrary.VSYNC_MODE_OFF, NativeLibrary.VSYNC_MODE_DOUBLE_BUFFERING, NativeLibrary.VSYNC_MODE_TRIPLE_BUFFERING)
                .map(vsync -> new SelectionAdapter.ChoiceItem<>(NativeLibrary.vsyncModeToResourceNameId(vsync), vsync))
                .collect(Collectors.toList());
        SelectionAdapter<Integer> vsyncSelectionAdapter = new SelectionAdapter<>(vsyncChoices, vsyncMode);
        SingleSelectionRecyclerViewItem<Integer> vsyncModeSelection = new SingleSelectionRecyclerViewItem<>(getString(R.string.vsync),
                getString(NativeLibrary.vsyncModeToResourceNameId(vsyncMode)), vsyncSelectionAdapter,
                (vsync, selectionRecyclerViewItem) -> {
                    NativeLibrary.setVSyncMode(vsync);
                    selectionRecyclerViewItem.setDescription(getString(NativeLibrary.vsyncModeToResourceNameId(vsync)));
                });
        genericRecyclerViewAdapter.addRecyclerViewItem(vsyncModeSelection);

        CheckboxRecyclerViewItem accurateBarriersCheckbox = new CheckboxRecyclerViewItem(getString(R.string.accurate_barriers), getString(R.string.accurate_barriers_description), NativeLibrary.getAccurateBarriers(), NativeLibrary::setAccurateBarriers);
        genericRecyclerViewAdapter.addRecyclerViewItem(accurateBarriersCheckbox);

        binding.graphicsSettingsRecyclerView.setAdapter(genericRecyclerViewAdapter);
        return binding.getRoot();
    }
}