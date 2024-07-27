package info.cemu.Cemu.guibasecomponents;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;

import info.cemu.Cemu.R;

public class SimpleButtonRecyclerViewItem implements RecyclerViewItem {
    public interface OnButtonClickListener {
        void onButtonClick();
    }

    private static class SimpleButtonViewHolder extends RecyclerView.ViewHolder {
        TextView text;

        public SimpleButtonViewHolder(View itemView) {
            super(itemView);
            text = itemView.findViewById(R.id.simple_button_text);
        }
    }

    private final SimpleButtonRecyclerViewItem.OnButtonClickListener onButtonClickListener;
    private final String text;

    public SimpleButtonRecyclerViewItem(String text, OnButtonClickListener onButtonClickListener) {
        this.text = text;
        this.onButtonClickListener = onButtonClickListener;
    }

    @Override
    public RecyclerView.ViewHolder onCreateViewHolder(ViewGroup parent) {
        return new SimpleButtonRecyclerViewItem.SimpleButtonViewHolder(LayoutInflater.from(parent.getContext()).inflate(R.layout.layout_simple_button, parent, false));
    }

    @Override
    public void onBindViewHolder(RecyclerView.ViewHolder viewHolder, RecyclerView.Adapter<RecyclerView.ViewHolder> adapter) {
        SimpleButtonRecyclerViewItem.SimpleButtonViewHolder buttonViewHolder = (SimpleButtonRecyclerViewItem.SimpleButtonViewHolder) viewHolder;
        buttonViewHolder.text.setText(text);
        buttonViewHolder.itemView.setOnClickListener(view -> {
            if (onButtonClickListener != null)
                onButtonClickListener.onButtonClick();
        });
    }
}
