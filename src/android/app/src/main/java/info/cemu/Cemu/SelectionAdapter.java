package info.cemu.Cemu;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;

import com.google.android.material.radiobutton.MaterialRadioButton;

import java.util.ArrayList;
import java.util.List;
import java.util.OptionalInt;
import java.util.stream.IntStream;

public class SelectionAdapter<T> extends BaseAdapter {
    public static class ChoiceItem<T> {
        int choiceTextResourceId;
        T choiceValue;

        public ChoiceItem(int choiceTextResourceId, T choiceValue) {
            this.choiceTextResourceId = choiceTextResourceId;
            this.choiceValue = choiceValue;
        }
    }

    protected List<ChoiceItem<T>> choiceItems = new ArrayList<>();
    protected int selectedPosition = 0;

    public SelectionAdapter() {
        super();
    }

    public SelectionAdapter(List<ChoiceItem<T>> choiceItems, T selectedValue) {
        super();
        this.choiceItems = choiceItems;
        setSelectedValue(selectedValue);
    }

    public void setSelectedValue(T selectedValue) {
        selectedPosition = getPositionOf(selectedValue);
    }

    public int getPositionOf(T value) {
        OptionalInt optionalInt = IntStream.range(0, choiceItems.size()).filter(position -> choiceItems.get(position).choiceValue.equals(value)).findFirst();
        if (!optionalInt.isPresent())
            throw new RuntimeException("value " + value + " was not found in the list of choiceItems");
        return optionalInt.getAsInt();
    }

    @Override
    public int getCount() {
        return choiceItems.size();
    }

    @Override
    public T getItem(int position) {
        return choiceItems.get(position).choiceValue;
    }

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
        radioButton.setText(choiceItems.get(position).choiceTextResourceId);
        radioButton.setChecked(position == selectedPosition);
        return view;
    }
}
