package info.cemu.Cemu.guibasecomponents;

import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import java.util.ArrayList;
import java.util.List;

public class GenericRecyclerViewAdapter extends RecyclerView.Adapter<RecyclerView.ViewHolder> {

    private final List<RecyclerViewItem> recyclerViewItems = new ArrayList<>();

    public void addRecyclerViewItem(RecyclerViewItem recyclerViewItem) {
        recyclerViewItems.add(recyclerViewItem);
        notifyItemInserted(recyclerViewItems.size() - 1);
    }

    public void removeRecyclerViewItem(RecyclerViewItem recyclerViewItem) {
        int position = recyclerViewItems.indexOf(recyclerViewItem);
        if (position == -1) return;
        recyclerViewItems.remove(position);
        notifyItemRemoved(position);
    }

    public void clearRecyclerViewItems() {
        int itemsCount = recyclerViewItems.size();
        recyclerViewItems.clear();
        notifyItemRangeRemoved(0, itemsCount);
    }

    @NonNull
    @Override
    public RecyclerView.ViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int position) {
        return recyclerViewItems.get(position).onCreateViewHolder(parent);
    }

    @Override
    public int getItemViewType(int position) {
        return position;
    }

    @Override
    public void onBindViewHolder(@NonNull RecyclerView.ViewHolder holder, int position) {
        recyclerViewItems.get(position).onBindViewHolder(holder, this);
    }

    @Override
    public int getItemCount() {
        return recyclerViewItems.size();
    }
}
