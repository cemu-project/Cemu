package info.cemu.Cemu.settings.input;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;

import info.cemu.Cemu.R;
import info.cemu.Cemu.guibasecomponents.RecyclerViewItem;

public class InputRecyclerViewItem implements RecyclerViewItem {
    public interface OnInputClickListener {
        void onInputClick(InputRecyclerViewItem inputRecyclerViewItem);
    }

    private static class InputViewHolder extends RecyclerView.ViewHolder {
        TextView inputName;
        TextView boundInput;

        public InputViewHolder(View itemView) {
            super(itemView);
            inputName = itemView.findViewById(R.id.controller_input_name);
            boundInput = itemView.findViewById(R.id.controller_button_name);
        }
    }

    private final int inputNameResourceId;
    private String boundInput;
    private final OnInputClickListener onInputClickListener;
    private RecyclerView.Adapter<RecyclerView.ViewHolder> recyclerViewAdapter;
    private InputViewHolder inputViewHolder;

    public InputRecyclerViewItem(int inputNameResourceId, String boundInput, OnInputClickListener onInputClickListener) {
        this.inputNameResourceId = inputNameResourceId;
        this.boundInput = boundInput;
        this.onInputClickListener = onInputClickListener;

    }

    public void clearBoundInput() {
        setBoundInput("");
    }

    public void setBoundInput(String boundInput) {
        this.boundInput = boundInput;
        recyclerViewAdapter.notifyItemChanged(inputViewHolder.getAdapterPosition());
    }

    @Override
    public RecyclerView.ViewHolder onCreateViewHolder(ViewGroup parent) {
        return new InputViewHolder(LayoutInflater.from(parent.getContext()).inflate(R.layout.layout_controller_input, parent, false));
    }

    @Override
    public void onBindViewHolder(RecyclerView.ViewHolder viewHolder, RecyclerView.Adapter<RecyclerView.ViewHolder> adapter) {
        inputViewHolder = (InputViewHolder) viewHolder;
        recyclerViewAdapter = adapter;
        inputViewHolder.itemView.setOnClickListener(view -> onInputClickListener.onInputClick(this));
        inputViewHolder.inputName.setText(inputNameResourceId);
        inputViewHolder.boundInput.setText(boundInput);
    }
}
