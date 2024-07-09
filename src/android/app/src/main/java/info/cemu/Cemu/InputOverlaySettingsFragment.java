package info.cemu.Cemu;


import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

import java.util.stream.Collectors;
import java.util.stream.IntStream;

import info.cemu.Cemu.databinding.GenericRecyclerViewLayoutBinding;


public class InputOverlaySettingsFragment extends Fragment {
    private InputOverlaySettingsProvider.OverlaySettings overlaySettings;

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (overlaySettings == null) {
            var context = requireContext();
            var sharedPref = context.getSharedPreferences(
                    context.getString(R.string.inputoverlaysettings_shared_preferences_name), Context.MODE_PRIVATE);
            InputOverlaySettingsProvider inputOverlaySettingsProvider = new InputOverlaySettingsProvider(sharedPref);
            overlaySettings = inputOverlaySettingsProvider.getOverlaySettings();
        }
    }

    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        var binding = GenericRecyclerViewLayoutBinding.inflate(inflater, container, false);
        GenericRecyclerViewAdapter genericRecyclerViewAdapter = new GenericRecyclerViewAdapter();

        CheckboxRecyclerViewItem inputOverlayCheckbox = new CheckboxRecyclerViewItem(
                getString(R.string.input_overlay),
                getString(R.string.enable_input_overlay),
                overlaySettings.isOverlayEnabled(),
                checked -> {
                    overlaySettings.setOverlayEnabled(checked);
                    overlaySettings.saveSettings();
                });
        genericRecyclerViewAdapter.addRecyclerViewItem(inputOverlayCheckbox);

        CheckboxRecyclerViewItem vibrateOnTouchCheckbox = new CheckboxRecyclerViewItem(
                getString(R.string.vibrate),
                getString(R.string.enable_vibrate_on_touch),
                overlaySettings.isVibrateOnTouchEnabled(),
                checked -> {
                    overlaySettings.setVibrateOnTouchEnabled(checked);
                    overlaySettings.saveSettings();
                });
        genericRecyclerViewAdapter.addRecyclerViewItem(vibrateOnTouchCheckbox);

        SliderRecyclerViewItem alphaSlider = new SliderRecyclerViewItem(
                getString(R.string.alpha_slider),
                0,
                255,
                overlaySettings.getAlpha(),
                value -> {
                    overlaySettings.setAlpha((int) value);
                    overlaySettings.saveSettings();
                });
        genericRecyclerViewAdapter.addRecyclerViewItem(alphaSlider);

        SelectionAdapter<Integer> controllerAdapter = new SelectionAdapter<>(
                IntStream.range(0, NativeLibrary.MAX_CONTROLLERS)
                        .mapToObj(i -> new SelectionAdapter.ChoiceItem<>(t -> t.setText(getString(R.string.controller_numbered, i + 1)), i))
                        .collect(Collectors.toList()),
                overlaySettings.getControllerIndex()
        );
        SingleSelectionRecyclerViewItem<Integer> controllerSelection = new SingleSelectionRecyclerViewItem<>(
                getString(R.string.overlay_controller),
                getString(R.string.controller_numbered, overlaySettings.getControllerIndex() + 1),
                controllerAdapter,
                (controllerIndex, selectionRecyclerViewItem) -> {
                    overlaySettings.setControllerIndex(controllerIndex);
                    selectionRecyclerViewItem.setDescription(getString(R.string.controller_numbered, controllerIndex + 1));
                    overlaySettings.saveSettings();
                });
        genericRecyclerViewAdapter.addRecyclerViewItem(controllerSelection);

        genericRecyclerViewAdapter.addRecyclerViewItem(
                new ButtonRecyclerViewItem(
                        getString(R.string.configure_inputs_label),
                        getString(R.string.configure_inputs_description),
                        () -> startActivity(new Intent(requireActivity(), InputOverlayConfigureActivity.class))
                )
        );

        binding.recyclerView.setAdapter(genericRecyclerViewAdapter);

        return binding.getRoot();
    }
}