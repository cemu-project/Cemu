package info.cemu.Cemu.guibasecomponents;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;

import com.google.android.material.slider.LabelFormatter;
import com.google.android.material.slider.Slider;

import info.cemu.Cemu.R;

public class SliderRecyclerViewItem implements RecyclerViewItem {
    public interface OnChangeListener {
        void onChange(float value);
    }

    private static class SliderViewHolder extends RecyclerView.ViewHolder {
        TextView label;
        Slider slider;

        public SliderViewHolder(View itemView) {
            super(itemView);
            label = itemView.findViewById(R.id.slider_label);
            slider = itemView.findViewById(R.id.slider);
        }
    }

    private final String label;
    private final float valueFrom;
    private final float valueTo;
    private float currentValue;
    private final OnChangeListener onChangeListener;
    private LabelFormatter labelFormatter;

    public SliderRecyclerViewItem(String label, float valueFrom, float valueTo, float currentValue, OnChangeListener onChangeListener, LabelFormatter labelFormatter) {
        this.label = label;
        this.valueFrom = valueFrom;
        this.valueTo = valueTo;
        this.currentValue = currentValue;
        this.onChangeListener = onChangeListener;
        this.labelFormatter = labelFormatter;
    }

    public SliderRecyclerViewItem(String label, float valueFrom, float valueTo, float currentValue, OnChangeListener onChangeListener) {
        this.label = label;
        this.valueFrom = valueFrom;
        this.valueTo = valueTo;
        this.currentValue = currentValue;
        this.onChangeListener = onChangeListener;
    }

    @Override
    public RecyclerView.ViewHolder onCreateViewHolder(ViewGroup parent) {
        return new SliderViewHolder(LayoutInflater.from(parent.getContext()).inflate(R.layout.layout_slider, parent, false));
    }

    @Override
    public void onBindViewHolder(RecyclerView.ViewHolder viewHolder, RecyclerView.Adapter<RecyclerView.ViewHolder> adapter) {
        SliderViewHolder sliderViewHolder = (SliderViewHolder) viewHolder;
        sliderViewHolder.label.setText(label);
        if (labelFormatter != null) sliderViewHolder.slider.setLabelFormatter(labelFormatter);
        sliderViewHolder.slider.setValueFrom(valueFrom);
        sliderViewHolder.slider.setValueTo(valueTo);
        float stepSize = 1.0f;
        sliderViewHolder.slider.setStepSize(stepSize);
        sliderViewHolder.slider.setValue(currentValue);
        sliderViewHolder.slider.addOnChangeListener((slider, value, fromUser) -> {
            currentValue = value;
            if (onChangeListener != null) onChangeListener.onChange(currentValue);
        });
    }
}
