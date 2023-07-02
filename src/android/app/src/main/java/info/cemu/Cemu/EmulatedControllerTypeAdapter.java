package info.cemu.Cemu;


import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.TextView;

import java.util.List;

public class EmulatedControllerTypeAdapter extends BaseAdapter {
    private final List<Integer> controllerTypes = List.of(NativeLibrary.EMULATED_CONTROLLER_TYPE_DISABLED, NativeLibrary.EMULATED_CONTROLLER_TYPE_VPAD, NativeLibrary.EMULATED_CONTROLLER_TYPE_PRO, NativeLibrary.EMULATED_CONTROLLER_TYPE_CLASSIC, NativeLibrary.EMULATED_CONTROLLER_TYPE_WIIMOTE);
    private int vpadCount = 0;
    private int wpadCount = 0;
    private int currentControllerType = NativeLibrary.EMULATED_CONTROLLER_TYPE_DISABLED;

    public void setCurrentControllerType(int currentControllerType) {
        this.currentControllerType = currentControllerType;
        notifyDataSetChanged();
    }

    public void setControllerTypeCounts(int vpadCount, int wpadCount) {
        this.vpadCount = vpadCount;
        this.wpadCount = wpadCount;
        notifyDataSetChanged();
    }

    @Override
    public boolean isEnabled(int position) {
        int type = controllerTypes.get(position);
        if (type == NativeLibrary.EMULATED_CONTROLLER_TYPE_DISABLED)
            return true;
        if (type == NativeLibrary.EMULATED_CONTROLLER_TYPE_VPAD)
            return currentControllerType == NativeLibrary.EMULATED_CONTROLLER_TYPE_VPAD || vpadCount < NativeLibrary.MAX_VPAD_CONTROLLERS;
        boolean isWPAD = currentControllerType != NativeLibrary.EMULATED_CONTROLLER_TYPE_VPAD && currentControllerType != NativeLibrary.EMULATED_CONTROLLER_TYPE_DISABLED;
        return isWPAD || wpadCount < NativeLibrary.MAX_WPAD_CONTROLLERS;
    }

    @Override
    public int getCount() {
        return controllerTypes.size();
    }

    @Override
    public Integer getItem(int position) {
        return controllerTypes.get(position);
    }

    @Override
    public long getItemId(int position) {
        return position;
    }

    @Override
    public View getView(int position, View view, ViewGroup viewGroup) {
        int type = controllerTypes.get(position);
        if (view == null)
            view = LayoutInflater.from(viewGroup.getContext()).inflate(R.layout.layout_choice, viewGroup, false);
        TextView textView = view.findViewById(R.id.textview_choice);
        textView.setText(NativeLibrary.controllerTypeToResourceNameId(type));
        float DISABLED_ALPHA = 0.6f;
        if (!isEnabled(position))
            view.setAlpha(DISABLED_ALPHA);
        return view;
    }

    public int getPosition(int controllerType) {
        return controllerTypes.indexOf(controllerType);
    }
}
