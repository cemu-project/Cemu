package info.cemu.Cemu.guibasecomponents;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;

import com.google.android.material.radiobutton.MaterialRadioButton;

import info.cemu.Cemu.R;

public abstract class BaseSelectionAdapter<T> extends BaseAdapter {

    protected int selectedPosition = 0;

    public BaseSelectionAdapter() {
        super();
    }

    public void setSelectedValue(T selectedValue) {
        selectedPosition = getPositionOf(selectedValue);
    }

    public abstract int getPositionOf(T value);

    abstract protected void setRadioButtonText(MaterialRadioButton radioButton, int position);

    public abstract T getItem(int position);

    @Override
    public long getItemId(int position) {
        return position;
    }

    @Override
    public View getView(int position, View view, ViewGroup viewGroup) {
        if (view == null)
            view = LayoutInflater.from(viewGroup.getContext()).inflate(R.layout.layout_single_selection_item, viewGroup, false);
        MaterialRadioButton radioButton = view.findViewById(R.id.single_selection_item_radio_button);
        radioButton.setEnabled(isEnabled(position));
        setRadioButtonText(radioButton, position);
        radioButton.setChecked(position == selectedPosition);
        return view;
    }
}
