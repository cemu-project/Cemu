package info.cemu.Cemu.settings.overlay;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
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


public class OverlaySettingsFragment extends Fragment {
    private static int overlayScreenPositionToResourceNameId(int overlayScreenPosition) {
        return switch (overlayScreenPosition) {
            case NativeLibrary.OVERLAY_SCREEN_POSITION_DISABLED ->
                    R.string.overlay_position_disabled;
            case NativeLibrary.OVERLAY_SCREEN_POSITION_TOP_LEFT ->
                    R.string.overlay_position_top_left;
            case NativeLibrary.OVERLAY_SCREEN_POSITION_TOP_CENTER ->
                    R.string.overlay_position_top_center;
            case NativeLibrary.OVERLAY_SCREEN_POSITION_TOP_RIGHT ->
                    R.string.overlay_position_top_right;
            case NativeLibrary.OVERLAY_SCREEN_POSITION_BOTTOM_LEFT ->
                    R.string.overlay_position_bottom_left;
            case NativeLibrary.OVERLAY_SCREEN_POSITION_BOTTOM_CENTER ->
                    R.string.overlay_position_bottom_center;
            case NativeLibrary.OVERLAY_SCREEN_POSITION_BOTTOM_RIGHT ->
                    R.string.overlay_position_bottom_right;
            default ->
                    throw new IllegalArgumentException("Invalid overlay position: " + overlayScreenPosition);
        };
    }

    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        var binding = GenericRecyclerViewLayoutBinding.inflate(inflater, container, false);

        GenericRecyclerViewAdapter genericRecyclerViewAdapter = new GenericRecyclerViewAdapter();

        var overlayPositionChoices = Stream.of(
                        NativeLibrary.OVERLAY_SCREEN_POSITION_DISABLED,
                        NativeLibrary.OVERLAY_SCREEN_POSITION_TOP_LEFT,
                        NativeLibrary.OVERLAY_SCREEN_POSITION_TOP_CENTER,
                        NativeLibrary.OVERLAY_SCREEN_POSITION_TOP_RIGHT,
                        NativeLibrary.OVERLAY_SCREEN_POSITION_BOTTOM_LEFT,
                        NativeLibrary.OVERLAY_SCREEN_POSITION_BOTTOM_CENTER,
                        NativeLibrary.OVERLAY_SCREEN_POSITION_BOTTOM_RIGHT)
                .map(position -> new SelectionAdapter.ChoiceItem<>(t -> t.setText(overlayScreenPositionToResourceNameId(position)), position))
                .collect(Collectors.toList());
        int overlayPosition = NativeLibrary.getOverlayPosition();
        SelectionAdapter<Integer> tvChannelsSelectionAdapter = new SelectionAdapter<>(overlayPositionChoices, overlayPosition);
        SingleSelectionRecyclerViewItem<Integer> tvChannelsModeSelection = new SingleSelectionRecyclerViewItem<>(getString(R.string.overlay_position),
                getString(overlayScreenPositionToResourceNameId(overlayPosition)), tvChannelsSelectionAdapter,
                (position, selectionRecyclerViewItem) -> {
                    NativeLibrary.setOverlayPosition(position);
                    selectionRecyclerViewItem.setDescription(getString(overlayScreenPositionToResourceNameId(position)));
                });
        genericRecyclerViewAdapter.addRecyclerViewItem(tvChannelsModeSelection);

        CheckboxRecyclerViewItem overlayFps = new CheckboxRecyclerViewItem(getString(R.string.fps_label),
                getString(R.string.fps_description), NativeLibrary.isOverlayFPSEnabled(),
                NativeLibrary::setOverlayFPSEnabled);
        genericRecyclerViewAdapter.addRecyclerViewItem(overlayFps);

        CheckboxRecyclerViewItem drawCallsCheckbox = new CheckboxRecyclerViewItem(getString(R.string.draw_calls_per_frame_label),
                getString(R.string.draw_calls_per_frame_label_desription), NativeLibrary.isOverlayDrawCallsPerFrameEnabled(),
                NativeLibrary::setOverlayDrawCallsPerFrameEnabled);
        genericRecyclerViewAdapter.addRecyclerViewItem(drawCallsCheckbox);

        CheckboxRecyclerViewItem cpuUsageCheckbox = new CheckboxRecyclerViewItem(getString(R.string.cpu_usage_label),
                getString(R.string.cpu_usage_description), NativeLibrary.isOverlayCPUUsageEnabled(),
                NativeLibrary::setOverlayCPUUsageEnabled);
        genericRecyclerViewAdapter.addRecyclerViewItem(cpuUsageCheckbox);

        CheckboxRecyclerViewItem ramUsageCheckbox = new CheckboxRecyclerViewItem(getString(R.string.ram_usage_label),
                getString(R.string.ram_usage_description), NativeLibrary.isOverlayRAMUsageEnabled(),
                NativeLibrary::setOverlayRAMUsageEnabled);
        genericRecyclerViewAdapter.addRecyclerViewItem(ramUsageCheckbox);

        CheckboxRecyclerViewItem vramUsageCheckbox = new CheckboxRecyclerViewItem(getString(R.string.vram_usage_label),
                getString(R.string.vram_usage_description), NativeLibrary.isOverlayVRAMUsageEnabled(),
                NativeLibrary::setOverlayVRAMUsageEnabled);
        genericRecyclerViewAdapter.addRecyclerViewItem(vramUsageCheckbox);

        binding.recyclerView.setAdapter(genericRecyclerViewAdapter);

        return binding.getRoot();
    }
}