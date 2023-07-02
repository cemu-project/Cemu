package info.cemu.Cemu;

import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.ActionBar;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.fragment.app.Fragment;

import info.cemu.Cemu.databinding.FragmentControllerInputsBinding;

import java.util.HashMap;
import java.util.Objects;

public class ControllerInputsFragment extends Fragment {
    public static final String CONTROLLER_INDEX = "ControllerIndex";
    private int controllerIndex;
    private int controllerType;
    private final InputManager inputManager = new InputManager();
    private EmulatedControllerTypeAdapter emulatedControllerTypeAdapter;
    private ControllerListAdapter controllerListAdapter;

    private void onTypeChanged(int controllerType) {
        if (this.controllerType == controllerType) return;
        this.controllerType = controllerType;
        NativeLibrary.setControllerType(controllerIndex, controllerType);
        emulatedControllerTypeAdapter.setCurrentControllerType(controllerType);
        controllerListAdapter.setEmulatedControllerListItems(controllerType, new HashMap<>());
        emulatedControllerTypeAdapter.setControllerTypeCounts(NativeLibrary.getVPADControllersCount(), NativeLibrary.getWPADControllersCount());
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
        controllerListAdapter = new ControllerListAdapter();
        emulatedControllerTypeAdapter = new EmulatedControllerTypeAdapter();
        emulatedControllerTypeAdapter.setCurrentControllerType(controllerType);
        emulatedControllerTypeAdapter.setControllerTypeCounts(NativeLibrary.getVPADControllersCount(), NativeLibrary.getWPADControllersCount());

        FragmentControllerInputsBinding binding = FragmentControllerInputsBinding.inflate(inflater, container, false);

        binding.emulatedControllerSpinner.setAdapter(emulatedControllerTypeAdapter);
        binding.emulatedControllerSpinner.setSelection(emulatedControllerTypeAdapter.getPosition(controllerType));
        binding.emulatedControllerSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> adapterView, View view, int position, long l) {
                onTypeChanged(emulatedControllerTypeAdapter.getItem(position));
            }

            @Override
            public void onNothingSelected(AdapterView<?> adapterView) {
            }
        });

        controllerListAdapter.setEmulatedControllerListItems(controllerType, NativeLibrary.getControllerMappings(controllerIndex));
        controllerListAdapter.setOnInputClickListener((nameId, id, position) -> {
            MotionDialog inputDialog = new MotionDialog(getContext());
            inputDialog.setOnKeyListener((dialog, keyCode, keyEvent) -> {
                if (inputManager.mapKeyEventToMappingId(controllerIndex, id, keyEvent)) {
                    controllerListAdapter.setBoundInput(position, NativeLibrary.getControllerMapping(controllerIndex, id));
                    inputDialog.dismiss();
                }
                return true;
            });
            inputDialog.setOnMotionListener(motionEvent -> {
                if (inputManager.mapMotionEventToMappingId(controllerIndex, id, motionEvent)) {
                    controllerListAdapter.setBoundInput(position, NativeLibrary.getControllerMapping(controllerIndex, id));
                    inputDialog.dismiss();
                }
                return true;
            });
            inputDialog.setMessage(getString(R.string.inputBindingDialogMessage, getString(nameId)));
            inputDialog.setButton(AlertDialog.BUTTON_NEUTRAL, getString(R.string.clear), (dialogInterface, i) -> {
                NativeLibrary.clearControllerMapping(controllerIndex, id);
                controllerListAdapter.clearBoundInput(position);
                dialogInterface.dismiss();
            });
            inputDialog.setButton(AlertDialog.BUTTON_NEGATIVE, getString(R.string.cancel), (dialogInterface, i) -> dialogInterface.dismiss());
            inputDialog.setTitle(R.string.inputBindingDialogTitle);
            inputDialog.show();
        });
        binding.controllerInputsRecyclerView.setAdapter(controllerListAdapter);

        return binding.getRoot();
    }
}