package info.cemu.Cemu.guibasecomponents;

import com.google.android.material.radiobutton.MaterialRadioButton;

import java.util.List;
import java.util.OptionalInt;
import java.util.stream.IntStream;

public class StringSelectionAdapter extends BaseSelectionAdapter<String> {
    private final List<String> choices;

    public StringSelectionAdapter(List<String> choices, String selectedValue) {
        super();
        this.choices = choices;
        setSelectedValue(selectedValue);
    }

    @Override
    public void setSelectedValue(String selectedValue) {
        selectedPosition = getPositionOf(selectedValue);
    }

    @Override
    public int getPositionOf(String value) {
        OptionalInt optionalInt = IntStream.range(0, choices.size()).filter(position -> choices.get(position).equals(value)).findFirst();
        if (!optionalInt.isPresent())
            throw new IllegalArgumentException("value " + value + " was not found in the list of choices");
        return optionalInt.getAsInt();
    }

    @Override
    protected void setRadioButtonText(MaterialRadioButton radioButton, int position) {
        radioButton.setText(choices.get(position));
    }

    @Override
    public int getCount() {
        return choices.size();
    }

    @Override
    public String getItem(int position) {
        return choices.get(position);
    }
}
