package info.cemu.Cemu.settings.input;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.ActionBar;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.fragment.app.Fragment;

import java.util.HashMap;
import java.util.Map;

import info.cemu.Cemu.R;
import info.cemu.Cemu.databinding.GenericRecyclerViewLayoutBinding;
import info.cemu.Cemu.guibasecomponents.GenericRecyclerViewAdapter;
import info.cemu.Cemu.guibasecomponents.HeaderRecyclerViewItem;
import info.cemu.Cemu.guibasecomponents.SingleSelectionRecyclerViewItem;
import info.cemu.Cemu.input.InputManager;
import info.cemu.Cemu.NativeLibrary;

public class ControllerInputsFragment extends Fragment {
    public static final String CONTROLLER_INDEX = "ControllerIndex";
    private int controllerIndex;
    private int controllerType;
    private final InputManager inputManager = new InputManager();
    private final ControllerInputsDataProvider controllerInputsDataProvider = new ControllerInputsDataProvider();
    private final GenericRecyclerViewAdapter genericRecyclerViewAdapter = new GenericRecyclerViewAdapter();
    private final EmulatedControllerTypeAdapter emulatedControllerTypeAdapter = new EmulatedControllerTypeAdapter();

    private void onTypeChanged(int controllerType) {
        if (this.controllerType == controllerType) return;
        this.controllerType = controllerType;
        NativeLibrary.setControllerType(controllerIndex, controllerType);
        genericRecyclerViewAdapter.clearRecyclerViewItems();
        setControllerInputs(new HashMap<>());
    }

    private void setControllerInputs(Map<Integer, String> inputs) {
        emulatedControllerTypeAdapter.setSelectedValue(controllerType);
        emulatedControllerTypeAdapter.setControllerTypeCounts(NativeLibrary.getVPADControllersCount(), NativeLibrary.getWPADControllersCount());
        String controllerTypeName = getString(ControllerTypeResourceNameMapper.controllerTypeToResourceNameId(controllerType));

        SingleSelectionRecyclerViewItem<Integer> emulatedControllerSelection = new SingleSelectionRecyclerViewItem<>(getString(R.string.emulated_controller_label),
                getString(R.string.emulated_controller_selection_description, controllerTypeName),
                emulatedControllerTypeAdapter,
                (controllerType, selectionRecyclerViewItem) -> onTypeChanged(controllerType));
        genericRecyclerViewAdapter.addRecyclerViewItem(emulatedControllerSelection);
        for (var controllerInputsGroups : controllerInputsDataProvider.getControllerInputs(controllerType, inputs)) {
            genericRecyclerViewAdapter.addRecyclerViewItem(new HeaderRecyclerViewItem(controllerInputsGroups.groupResourceIdName));
            for (var controllerInput : controllerInputsGroups.inputs) {
                int buttonId = controllerInput.buttonId;
                int buttonResourceIdName = controllerInput.buttonResourceIdName;
                InputRecyclerViewItem inputRecyclerViewItem = new InputRecyclerViewItem(controllerInput.buttonResourceIdName, controllerInput.boundInput, inputItem -> {
                    MotionDialog inputDialog = new MotionDialog(getContext());
                    inputDialog.setOnKeyListener((dialog, keyCode, keyEvent) -> {
                        if (inputManager.mapKeyEventToMappingId(controllerIndex, buttonId, keyEvent)) {
                            inputItem.setBoundInput(NativeLibrary.getControllerMapping(controllerIndex, buttonId));
                            inputDialog.dismiss();
                        }
                        return true;
                    });
                    inputDialog.setOnMotionListener(motionEvent -> {
                        if (inputManager.mapMotionEventToMappingId(controllerIndex, buttonId, motionEvent)) {
                            inputItem.setBoundInput(NativeLibrary.getControllerMapping(controllerIndex, buttonId));
                            inputDialog.dismiss();
                        }
                        return true;
                    });
                    inputDialog.setMessage(getString(R.string.inputBindingDialogMessage, getString(buttonResourceIdName)));
                    inputDialog.setButton(AlertDialog.BUTTON_NEUTRAL, getString(R.string.clear), (dialogInterface, i) -> {
                        NativeLibrary.clearControllerMapping(controllerIndex, buttonId);
                        inputItem.clearBoundInput();
                        dialogInterface.dismiss();
                    });
                    inputDialog.setButton(AlertDialog.BUTTON_NEGATIVE, getString(R.string.cancel), (dialogInterface, i) -> dialogInterface.dismiss());
                    inputDialog.setTitle(R.string.inputBindingDialogTitle);
                    inputDialog.show();
                });
                genericRecyclerViewAdapter.addRecyclerViewItem(inputRecyclerViewItem);
            }
        }
    }

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        controllerIndex = requireArguments().getInt(CONTROLLER_INDEX);
        ActionBar actionBar = ((AppCompatActivity) requireActivity()).getSupportActionBar();
        if (actionBar != null)
            actionBar.setTitle(getString(R.string.controller_numbered, controllerIndex + 1));
        if (NativeLibrary.isControllerDisabled(controllerIndex))
            controllerType = NativeLibrary.EMULATED_CONTROLLER_TYPE_DISABLED;
        else controllerType = NativeLibrary.getControllerType(controllerIndex);

        setControllerInputs(NativeLibrary.getControllerMappings(controllerIndex));

        var binding = GenericRecyclerViewLayoutBinding.inflate(inflater, container, false);
        binding.recyclerView.setAdapter(genericRecyclerViewAdapter);

        return binding.getRoot();
    }
}