package info.cemu.Cemu.settings.input;


import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.navigation.fragment.NavHostFragment;

import info.cemu.Cemu.R;
import info.cemu.Cemu.databinding.GenericRecyclerViewLayoutBinding;
import info.cemu.Cemu.guibasecomponents.ButtonRecyclerViewItem;
import info.cemu.Cemu.guibasecomponents.GenericRecyclerViewAdapter;
import info.cemu.Cemu.guibasecomponents.SimpleButtonRecyclerViewItem;
import info.cemu.Cemu.NativeLibrary;

public class InputSettingsFragment extends Fragment {

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        var binding = GenericRecyclerViewLayoutBinding.inflate(inflater, container, false);
        GenericRecyclerViewAdapter genericRecyclerViewAdapter = new GenericRecyclerViewAdapter();
        genericRecyclerViewAdapter.addRecyclerViewItem(new SimpleButtonRecyclerViewItem(getString(R.string.input_overlay_settings), () -> NavHostFragment.findNavController(InputSettingsFragment.this).navigate(R.id.action_inputSettingsFragment_to_inputOverlaySettingsFragment)));

        for (int index = 0; index < NativeLibrary.MAX_CONTROLLERS; index++) {
            int controllerIndex = index;
            int controllerType = NativeLibrary.isControllerDisabled(controllerIndex) ? NativeLibrary.EMULATED_CONTROLLER_TYPE_DISABLED : NativeLibrary.getControllerType(controllerIndex);
            String controllerTypeName = getString(ControllerTypeResourceNameMapper.controllerTypeToResourceNameId(controllerType));
            ButtonRecyclerViewItem buttonRecyclerViewItem = new ButtonRecyclerViewItem(
                    getString(R.string.controller_numbered, controllerIndex + 1),
                    getString(R.string.emulated_controller_with_type, controllerTypeName),
                    () -> {
                        Bundle bundle = new Bundle();
                        bundle.putInt(ControllerInputsFragment.CONTROLLER_INDEX, controllerIndex);
                        NavHostFragment.findNavController(InputSettingsFragment.this).navigate(R.id.action_inputSettingsFragment_to_controllerInputsFragment, bundle);
                    });
            genericRecyclerViewAdapter.addRecyclerViewItem(buttonRecyclerViewItem);
        }
        binding.recyclerView.setAdapter(genericRecyclerViewAdapter);
        return binding.getRoot();
    }
}
