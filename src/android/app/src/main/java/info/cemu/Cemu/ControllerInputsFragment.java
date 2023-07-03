package info.cemu.Cemu;

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

import info.cemu.Cemu.databinding.FragmentControllerInputsBinding;

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
        emulatedControllerTypeAdapter.setCurrentControllerType(controllerType);
        emulatedControllerTypeAdapter.setControllerTypeCounts(NativeLibrary.getVPADControllersCount(), NativeLibrary.getWPADControllersCount());
        SpinnerRecyclerViewItem emulatedControllerSpinner = new SpinnerRecyclerViewItem(R.string.emulated_controller_label, emulatedControllerTypeAdapter, emulatedControllerTypeAdapter.getPosition(controllerType), position -> onTypeChanged(emulatedControllerTypeAdapter.getItem(position)));
        genericRecyclerViewAdapter.addRecyclerViewItem(emulatedControllerSpinner);
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

        FragmentControllerInputsBinding binding = FragmentControllerInputsBinding.inflate(inflater, container, false);
        binding.controllerInputsRecyclerView.setAdapter(genericRecyclerViewAdapter);

        return binding.getRoot();
    }
}