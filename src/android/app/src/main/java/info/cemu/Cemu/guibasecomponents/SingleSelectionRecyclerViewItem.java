package info.cemu.Cemu.guibasecomponents;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.appcompat.app.AlertDialog;
import androidx.recyclerview.widget.RecyclerView;

import com.google.android.material.dialog.MaterialAlertDialogBuilder;

import info.cemu.Cemu.R;

public class SingleSelectionRecyclerViewItem<T> implements RecyclerViewItem {
    private static class SingleSelectionViewHolder extends RecyclerView.ViewHolder {
        TextView label;
        TextView description;

        public SingleSelectionViewHolder(View itemView) {
            super(itemView);
            label = itemView.findViewById(R.id.selection_label);
            description = itemView.findViewById(R.id.selection_description);
        }
    }

    public interface OnItemSelectedListener<T> {
        void onItemSelected(T selectedValue, SingleSelectionRecyclerViewItem<T> selectionRecyclerViewItem);
    }

    private final BaseSelectionAdapter<T> selectionAdapter;
    private final String label;
    private String description;
    private final OnItemSelectedListener<T> onItemSelectedListener;
    private SingleSelectionViewHolder singleSelectionViewHolder;
    private RecyclerView.Adapter<RecyclerView.ViewHolder> recyclerViewAdapter;
    private AlertDialog selectAlertDialog;

    public SingleSelectionRecyclerViewItem(String label, String description, BaseSelectionAdapter<T> selectionAdapter, OnItemSelectedListener<T> onItemSelectedListener) {
        this.label = label;
        this.description = description;
        this.selectionAdapter = selectionAdapter;
        this.onItemSelectedListener = onItemSelectedListener;
    }

    public void setDescription(String description) {
        this.description = description;
        recyclerViewAdapter.notifyItemChanged(singleSelectionViewHolder.getAdapterPosition());
    }

    @Override
    public RecyclerView.ViewHolder onCreateViewHolder(ViewGroup parent) {
        return new SingleSelectionViewHolder(LayoutInflater.from(parent.getContext()).inflate(R.layout.layout_single_selection, parent, false));
    }

    @Override
    public void onBindViewHolder(RecyclerView.ViewHolder viewHolder, RecyclerView.Adapter<RecyclerView.ViewHolder> recyclerViewAdapter) {
        this.recyclerViewAdapter = recyclerViewAdapter;
        singleSelectionViewHolder = (SingleSelectionViewHolder) viewHolder;
        singleSelectionViewHolder.label.setText(label);
        singleSelectionViewHolder.description.setText(description);
        singleSelectionViewHolder.itemView.setOnClickListener(view -> {
            if (selectAlertDialog == null) {
                MaterialAlertDialogBuilder builder = new MaterialAlertDialogBuilder(viewHolder.itemView.getContext());
                selectAlertDialog = builder.setTitle(label).setAdapter(selectionAdapter, (dialogInterface, position) -> {
                    if (!selectionAdapter.isEnabled(position))
                        return;
                    T selectedValue = selectionAdapter.getItem(position);
                    selectionAdapter.setSelectedValue(selectedValue);
                    if (onItemSelectedListener != null)
                        onItemSelectedListener.onItemSelected(selectedValue, SingleSelectionRecyclerViewItem.this);

                }).setNegativeButton(R.string.cancel, (dialogInterface, i) -> dialogInterface.dismiss()).show();
                return;
            }
            selectAlertDialog.show();
        });
    }
}
