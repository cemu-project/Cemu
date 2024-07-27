package info.cemu.Cemu.guibasecomponents;

import android.view.ViewGroup;

import androidx.recyclerview.widget.RecyclerView;

public interface RecyclerViewItem {
    RecyclerView.ViewHolder onCreateViewHolder(ViewGroup parent);

    void onBindViewHolder(RecyclerView.ViewHolder viewHolder, RecyclerView.Adapter<RecyclerView.ViewHolder> adapter);
}
