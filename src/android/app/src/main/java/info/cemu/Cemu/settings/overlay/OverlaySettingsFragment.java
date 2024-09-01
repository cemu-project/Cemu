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
import info.cemu.Cemu.guibasecomponents.HeaderRecyclerViewItem;
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

        genericRecyclerViewAdapter.addRecyclerViewItem(new HeaderRecyclerViewItem(R.string.overlay));
        SelectionAdapter<Integer> overlayPositionSelectionAdapter = new SelectionAdapter<>(overlayPositionChoices, overlayPosition);
        SingleSelectionRecyclerViewItem<Integer> overlayPositionSelection = new SingleSelectionRecyclerViewItem<>(getString(R.string.overlay_position),
                getString(overlayScreenPositionToResourceNameId(overlayPosition)), overlayPositionSelectionAdapter,
                (position, selectionRecyclerViewItem) -> {
                    NativeLibrary.setOverlayPosition(position);
                    selectionRecyclerViewItem.setDescription(getString(overlayScreenPositionToResourceNameId(position)));
                });
        genericRecyclerViewAdapter.addRecyclerViewItem(overlayPositionSelection);

        CheckboxRecyclerViewItem overlayFps = new CheckboxRecyclerViewItem(getString(R.string.fps),
                getString(R.string.fps_overlay_description), NativeLibrary.isOverlayFPSEnabled(),
                NativeLibrary::setOverlayFPSEnabled);
        genericRecyclerViewAdapter.addRecyclerViewItem(overlayFps);

        CheckboxRecyclerViewItem drawCallsCheckbox = new CheckboxRecyclerViewItem(getString(R.string.draw_calls_per_frame),
                getString(R.string.draw_calls_per_frame_overlay_description), NativeLibrary.isOverlayDrawCallsPerFrameEnabled(),
                NativeLibrary::setOverlayDrawCallsPerFrameEnabled);
        genericRecyclerViewAdapter.addRecyclerViewItem(drawCallsCheckbox);

        CheckboxRecyclerViewItem cpuUsageCheckbox = new CheckboxRecyclerViewItem(getString(R.string.cpu_usage),
                getString(R.string.cpu_usage_overlay_description), NativeLibrary.isOverlayCPUUsageEnabled(),
                NativeLibrary::setOverlayCPUUsageEnabled);
        genericRecyclerViewAdapter.addRecyclerViewItem(cpuUsageCheckbox);

        CheckboxRecyclerViewItem ramUsageCheckbox = new CheckboxRecyclerViewItem(getString(R.string.ram_usage),
                getString(R.string.ram_usage_overlay_description), NativeLibrary.isOverlayRAMUsageEnabled(),
                NativeLibrary::setOverlayRAMUsageEnabled);
        genericRecyclerViewAdapter.addRecyclerViewItem(ramUsageCheckbox);

        CheckboxRecyclerViewItem debugCheckbox = new CheckboxRecyclerViewItem(getString(R.string.debug),
                getString(R.string.debug_overlay_description), NativeLibrary.isOverlayDebugEnabled(),
                NativeLibrary::setOverlayDebugEnabled);
        genericRecyclerViewAdapter.addRecyclerViewItem(debugCheckbox);

        genericRecyclerViewAdapter.addRecyclerViewItem(new HeaderRecyclerViewItem(R.string.notifications));
        int notificationsPosition = NativeLibrary.getNotificationsPosition();
        SelectionAdapter<Integer> notificationsPositionSelectionAdapter = new SelectionAdapter<>(overlayPositionChoices, notificationsPosition);
        SingleSelectionRecyclerViewItem<Integer> notificationsPositionSelection = new SingleSelectionRecyclerViewItem<>(getString(R.string.overlay_position),
                getString(overlayScreenPositionToResourceNameId(notificationsPosition)), notificationsPositionSelectionAdapter,
                (position, selectionRecyclerViewItem) -> {
                    NativeLibrary.setNotificationsPosition(position);
                    selectionRecyclerViewItem.setDescription(getString(overlayScreenPositionToResourceNameId(position)));
                });
        genericRecyclerViewAdapter.addRecyclerViewItem(notificationsPositionSelection);

        CheckboxRecyclerViewItem controllerProfiles = new CheckboxRecyclerViewItem(getString(R.string.controller_profiles),
                getString(R.string.controller_profiles_notification_description), NativeLibrary.isNotificationControllerProfilesEnabled(),
                NativeLibrary::setNotificationControllerProfilesEnabled);
        genericRecyclerViewAdapter.addRecyclerViewItem(controllerProfiles);

        CheckboxRecyclerViewItem shaderCompiler = new CheckboxRecyclerViewItem(getString(R.string.shader_compiler),
                getString(R.string.shader_compiler_notification_description), NativeLibrary.isNotificationShaderCompilerEnabled(),
                NativeLibrary::setNotificationShaderCompilerEnabled);
        genericRecyclerViewAdapter.addRecyclerViewItem(shaderCompiler);

        CheckboxRecyclerViewItem friendList = new CheckboxRecyclerViewItem(getString(R.string.friend_list),
                getString(R.string.friend_list_notification_description), NativeLibrary.isNotificationFriendListEnabled(),
                NativeLibrary::setNotificationFriendListEnabled);
        genericRecyclerViewAdapter.addRecyclerViewItem(friendList);

        binding.recyclerView.setAdapter(genericRecyclerViewAdapter);

        return binding.getRoot();
    }
}