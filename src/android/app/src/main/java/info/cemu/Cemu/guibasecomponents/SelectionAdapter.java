package info.cemu.Cemu.guibasecomponents;

import android.widget.TextView;

import com.google.android.material.radiobutton.MaterialRadioButton;

import java.util.ArrayList;
import java.util.List;
import java.util.OptionalInt;
import java.util.function.Consumer;
import java.util.stream.IntStream;

public class SelectionAdapter<T> extends BaseSelectionAdapter<T> {
    public static class ChoiceItem<T> {
        Consumer<TextView> setTextForChoice;
        T choiceValue;

        public ChoiceItem(Consumer<TextView> setTextForChoice, T choiceValue) {
            this.setTextForChoice = setTextForChoice;
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
            throw new IllegalArgumentException("value " + value + " was not found in the list of choiceItems");
        return optionalInt.getAsInt();
    }

    @Override
    protected void setRadioButtonText(MaterialRadioButton radioButton, int position) {
        choiceItems.get(position).setTextForChoice.accept(radioButton);
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
