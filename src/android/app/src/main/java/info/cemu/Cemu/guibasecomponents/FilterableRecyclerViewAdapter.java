package info.cemu.Cemu.guibasecomponents;

import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Predicate;
import java.util.stream.Collectors;

public class FilterableRecyclerViewAdapter<T extends RecyclerViewItem> extends RecyclerView.Adapter<RecyclerView.ViewHolder> {

    private Predicate<T> predicate;
    private final List<T> recyclerViewItems = new ArrayList<>();
    private List<T> filteredRecyclerViewItems = new ArrayList<>();

    public void addRecyclerViewItem(T recyclerViewItem) {
        recyclerViewItems.add(recyclerViewItem);
        if (predicate == null || predicate.test(recyclerViewItem)) {
            filteredRecyclerViewItems.add(recyclerViewItem);
            notifyItemInserted(filteredRecyclerViewItems.size() - 1);
        }
    }

    public void setPredicate(@NonNull Predicate<T> predicate) {
        this.predicate = predicate;
        int oldSize = filteredRecyclerViewItems.size();
        filteredRecyclerViewItems = recyclerViewItems.stream().filter(predicate).collect(Collectors.toList());
        if (oldSize != filteredRecyclerViewItems.size())
            notifyDataSetChanged();
    }

    public void clearPredicate() {
        int oldSize = filteredRecyclerViewItems.size();
        filteredRecyclerViewItems = new ArrayList<>(recyclerViewItems);
        if (oldSize != filteredRecyclerViewItems.size())
            notifyDataSetChanged();
    }

    @NonNull
    @Override
    public RecyclerView.ViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int position) {
        return filteredRecyclerViewItems.get(position).onCreateViewHolder(parent);
    }

    @Override
    public void onBindViewHolder(@NonNull RecyclerView.ViewHolder holder, int position) {
        filteredRecyclerViewItems.get(position).onBindViewHolder(holder, this);
    }

    @Override
    public int getItemCount() {
        return filteredRecyclerViewItems.size();
    }

    public void clearRecyclerViewItems() {
        int size = filteredRecyclerViewItems.size();
        filteredRecyclerViewItems.clear();
        recyclerViewItems.clear();
        notifyItemRangeRemoved(0, size);
    }
}
