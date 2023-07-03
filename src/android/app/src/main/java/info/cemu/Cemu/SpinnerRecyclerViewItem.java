package info.cemu.Cemu;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.Spinner;
import android.widget.SpinnerAdapter;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;

public class SpinnerRecyclerViewItem implements RecyclerViewItem {
    public interface OnItemSelectedListener {
        void onItemSelected(int position);
    }

    private static class SpinnerViewHolder extends RecyclerView.ViewHolder {
        TextView text;
        Spinner spinner;

        public SpinnerViewHolder(View itemView) {
            super(itemView);
            text = itemView.findViewById(R.id.spinner_label);
            spinner = itemView.findViewById(R.id.spinner);
        }
    }

    private final SpinnerAdapter spinnerAdapter;
    private final int spinnerLabelResourceId;
    private final int initialSelection;
    private final OnItemSelectedListener onItemSelectedListener;

    public SpinnerRecyclerViewItem(int spinnerLabelResourceId, SpinnerAdapter spinnerAdapter, int initialSelection, OnItemSelectedListener onItemSelectedListener) {
        this.spinnerLabelResourceId = spinnerLabelResourceId;
        this.spinnerAdapter = spinnerAdapter;
        this.initialSelection = initialSelection;
        this.onItemSelectedListener = onItemSelectedListener;
    }

    @Override
    public RecyclerView.ViewHolder onCreateViewHolder(ViewGroup parent) {
        return new SpinnerViewHolder(LayoutInflater.from(parent.getContext()).inflate(R.layout.layout_spinner, parent, false));
    }

    @Override
    public void onBindViewHolder(RecyclerView.ViewHolder viewHolder, RecyclerView.Adapter<RecyclerView.ViewHolder> adapter) {
        SpinnerViewHolder spinnerViewHolder = (SpinnerViewHolder) viewHolder;
        spinnerViewHolder.text.setText(spinnerLabelResourceId);
        spinnerViewHolder.spinner.setAdapter(spinnerAdapter);
        spinnerViewHolder.spinner.setSelection(initialSelection);
        spinnerViewHolder.spinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> adapterView, View view, int position, long l) {
                if (onItemSelectedListener != null)
                    onItemSelectedListener.onItemSelected(position);
            }

            @Override
            public void onNothingSelected(AdapterView<?> adapterView) {

            }
        });
    }
}
