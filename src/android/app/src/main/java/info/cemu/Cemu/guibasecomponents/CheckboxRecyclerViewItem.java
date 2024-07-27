package info.cemu.Cemu.guibasecomponents;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;

import com.google.android.material.checkbox.MaterialCheckBox;

import info.cemu.Cemu.R;

public class CheckboxRecyclerViewItem implements RecyclerViewItem {
    public interface OnCheckedChangeListener {
        void onCheckChanged(boolean checked);
    }

    private static class CheckBoxViewHolder extends RecyclerView.ViewHolder {
        TextView label;
        TextView description;
        MaterialCheckBox checkBox;

        public CheckBoxViewHolder(View itemView) {
            super(itemView);
            label = itemView.findViewById(R.id.checkbox_label);
            description = itemView.findViewById(R.id.checkbox_description);
            checkBox = itemView.findViewById(R.id.checkbox);
        }
    }

    private final String label;
    private final String description;
    private boolean checked;
    private final OnCheckedChangeListener onCheckedChangeListener;

    public CheckboxRecyclerViewItem(String label, String description, boolean checked, OnCheckedChangeListener onCheckedChangeListener) {
        this.label = label;
        this.description = description;
        this.checked = checked;
        this.onCheckedChangeListener = onCheckedChangeListener;
    }

    @Override
    public RecyclerView.ViewHolder onCreateViewHolder(ViewGroup parent) {
        return new CheckBoxViewHolder(LayoutInflater.from(parent.getContext()).inflate(R.layout.layout_checkbox, parent, false));
    }

    @Override
    public void onBindViewHolder(RecyclerView.ViewHolder viewHolder, RecyclerView.Adapter<RecyclerView.ViewHolder> adapter) {
        CheckBoxViewHolder checkBoxViewHolder = (CheckBoxViewHolder) viewHolder;
        checkBoxViewHolder.label.setText(label);
        checkBoxViewHolder.description.setText(description);
        checkBoxViewHolder.checkBox.setChecked(checked);
        checkBoxViewHolder.itemView.setOnClickListener(view -> {
            checked = !checkBoxViewHolder.checkBox.isChecked();
            checkBoxViewHolder.checkBox.setChecked(checked);
            if (onCheckedChangeListener != null) onCheckedChangeListener.onCheckChanged(checked);
        });
    }
}
