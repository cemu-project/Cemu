package info.cemu.Cemu.guibasecomponents;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;

import info.cemu.Cemu.R;

public class ButtonRecyclerViewItem implements RecyclerViewItem {
    public interface OnButtonClickListener {
        void onButtonClick();
    }

    private static class ButtonViewHolder extends RecyclerView.ViewHolder {
        TextView text;
        TextView description;

        public ButtonViewHolder(View itemView) {
            super(itemView);
            text = itemView.findViewById(R.id.button_text);
            description = itemView.findViewById(R.id.button_description);
        }
    }

    private final OnButtonClickListener onButtonClickListener;
    private final String text;
    private final String description;

    public ButtonRecyclerViewItem(String text, String description, OnButtonClickListener onButtonClickListener) {
        this.text = text;
        this.description = description;
        this.onButtonClickListener = onButtonClickListener;
    }

    @Override
    public RecyclerView.ViewHolder onCreateViewHolder(ViewGroup parent) {
        return new ButtonViewHolder(LayoutInflater.from(parent.getContext()).inflate(R.layout.layout_button, parent, false));
    }

    @Override
    public void onBindViewHolder(RecyclerView.ViewHolder viewHolder, RecyclerView.Adapter<RecyclerView.ViewHolder> adapter) {
        ButtonViewHolder buttonViewHolder = (ButtonViewHolder) viewHolder;
        buttonViewHolder.text.setText(text);
        buttonViewHolder.description.setText(description);
        buttonViewHolder.itemView.setOnClickListener(view -> {
            if (onButtonClickListener != null)
                onButtonClickListener.onButtonClick();
        });
    }
}
