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

public class SelectionAdapter<T> extends BaseSelectionAdapter<T> {
    public static class ChoiceItem<T> {
        int choiceTextResourceId;
        T choiceValue;

        public ChoiceItem(int choiceTextResourceId, T choiceValue) {
            this.choiceTextResourceId = choiceTextResourceId;
            this.choiceValue = choiceValue;
        }
    }

    protected List<ChoiceItem<T>> choiceItems = new ArrayList<>();

    public SelectionAdapter() {
        super();
    }

    public SelectionAdapter(List<ChoiceItem<T>> choiceItems, T selectedValue) {
        super();
        this.choiceItems = choiceItems;
        setSelectedValue(selectedValue);
    }

    @Override
    public void setSelectedValue(T selectedValue) {
        selectedPosition = getPositionOf(selectedValue);
    }

    @Override
    public int getPositionOf(T value) {
        OptionalInt optionalInt = IntStream.range(0, choiceItems.size()).filter(position -> choiceItems.get(position).choiceValue.equals(value)).findFirst();
        if (!optionalInt.isPresent())
            throw new RuntimeException("value " + value + " was not found in the list of choiceItems");
        return optionalInt.getAsInt();
    }

    @Override
    protected void setRadioButtonText(MaterialRadioButton radioButton, int position) {
        radioButton.setText(choiceItems.get(position).choiceTextResourceId);
    }

    @Override
    public int getCount() {
        return choiceItems.size();
    }

    @Override
    public T getItem(int position) {
        return choiceItems.get(position).choiceValue;
    }
}
