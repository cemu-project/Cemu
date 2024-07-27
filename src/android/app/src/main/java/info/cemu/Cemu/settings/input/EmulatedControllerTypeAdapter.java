package info.cemu.Cemu.settings.input;


import java.util.stream.Collectors;
import java.util.stream.Stream;

import info.cemu.Cemu.guibasecomponents.SelectionAdapter;
import info.cemu.Cemu.NativeLibrary;

public class EmulatedControllerTypeAdapter extends SelectionAdapter<Integer> {
    private int vpadCount = 0;
    private int wpadCount = 0;

    public EmulatedControllerTypeAdapter() {
        choiceItems = Stream.of(NativeLibrary.EMULATED_CONTROLLER_TYPE_DISABLED,
                        NativeLibrary.EMULATED_CONTROLLER_TYPE_VPAD,
                        NativeLibrary.EMULATED_CONTROLLER_TYPE_PRO,
                        NativeLibrary.EMULATED_CONTROLLER_TYPE_CLASSIC,
                        NativeLibrary.EMULATED_CONTROLLER_TYPE_WIIMOTE)
                .map(type -> new ChoiceItem<>(t->t.setText(ControllerTypeResourceNameMapper.controllerTypeToResourceNameId(type)), type))
                .collect(Collectors.toList());
        setSelectedValue(NativeLibrary.EMULATED_CONTROLLER_TYPE_DISABLED);
    }

    public void setControllerTypeCounts(int vpadCount, int wpadCount) {
        this.vpadCount = vpadCount;
        this.wpadCount = wpadCount;
        notifyDataSetChanged();
    }

    @Override
    public boolean isEnabled(int position) {
        int currentControllerType = getItem(selectedPosition);
        int type = getItem(position);
        if (type == NativeLibrary.EMULATED_CONTROLLER_TYPE_DISABLED)
            return true;
        if (type == NativeLibrary.EMULATED_CONTROLLER_TYPE_VPAD)
            return currentControllerType == NativeLibrary.EMULATED_CONTROLLER_TYPE_VPAD || vpadCount < NativeLibrary.MAX_VPAD_CONTROLLERS;
        boolean isWPAD = currentControllerType != NativeLibrary.EMULATED_CONTROLLER_TYPE_VPAD && currentControllerType != NativeLibrary.EMULATED_CONTROLLER_TYPE_DISABLED;
        return isWPAD || wpadCount < NativeLibrary.MAX_WPAD_CONTROLLERS;
    }
}
