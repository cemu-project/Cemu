package info.cemu.Cemu;

import static java.util.regex.Pattern.CASE_INSENSITIVE;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.List;
import java.util.regex.Pattern;
import java.util.stream.Collectors;

public class GraphicPacksRecyclerViewAdapter extends RecyclerView.Adapter<RecyclerView.ViewHolder> {
    private static class GraphicPackSearchItemRecyclerViewItem implements RecyclerViewItem {

        private static class HeaderViewHolder extends RecyclerView.ViewHolder {
            TextView name;
            TextView path;

            public HeaderViewHolder(View itemView) {
                super(itemView);
                name = itemView.findViewById(R.id.graphic_pack_search_name);
                path = itemView.findViewById(R.id.graphic_pack_search_path);
            }
        }

        public interface OnClickCallback {
            void onClick();
        }

        private final String name;
        private final String path;
        private final OnClickCallback onClickCallback;

        private GraphicPackSearchItemRecyclerViewItem(String name, String path, OnClickCallback onClickCallback) {
            this.name = name;
            this.path = path;
            this.onClickCallback = onClickCallback;
        }

        @Override
        public RecyclerView.ViewHolder onCreateViewHolder(ViewGroup parent) {
            return new HeaderViewHolder(LayoutInflater.from(parent.getContext()).inflate(R.layout.layout_graphic_pack_search_item, parent, false));
        }

        @Override
        public void onBindViewHolder(RecyclerView.ViewHolder viewHolder, RecyclerView.Adapter<RecyclerView.ViewHolder> adapter) {
            HeaderViewHolder headerViewHolder = (HeaderViewHolder) viewHolder;
            headerViewHolder.itemView.setOnClickListener(v -> onClickCallback.onClick());
            headerViewHolder.name.setText(name);
            headerViewHolder.path.setText(path);
        }
    }

    public interface OnItemClickCallback {
        void onClick(GraphicPacksRootFragment.GraphicPackDataNode graphicPackDataNode);
    }

    private final OnItemClickCallback onItemClickCallback;

    public GraphicPacksRecyclerViewAdapter(OnItemClickCallback onItemClickCallback) {
        this.onItemClickCallback = onItemClickCallback;
    }

    private List<GraphicPackSearchItemRecyclerViewItem> recyclerViewItems = new ArrayList<>();
    private List<GraphicPackSearchItemRecyclerViewItem> filteredViewItems = new ArrayList<>();

    public void setItems(List<GraphicPacksRootFragment.GraphicPackDataNode> graphicPackDataNodes) {
        clearItems();
        recyclerViewItems = graphicPackDataNodes.stream()
                .sorted(Comparator.comparing(GraphicPacksRootFragment.GraphicPackDataNode::getPath))
                .map(graphicPackDataNode ->
                        new GraphicPackSearchItemRecyclerViewItem(
                                graphicPackDataNode.getName(),
                                graphicPackDataNode.getPath(),
                                () -> GraphicPacksRecyclerViewAdapter.this.onItemClickCallback.onClick(graphicPackDataNode)
                        )).collect(Collectors.toList());
        filteredViewItems = recyclerViewItems;
        notifyItemRangeInserted(0, filteredViewItems.size());
    }

    public void filter(String text) {
        if (text.isBlank()) {
            filteredViewItems = recyclerViewItems;
            notifyDataSetChanged();
            return;
        }
        var stringBuilder = new StringBuilder();
        String pattern = Arrays.stream(text.split(" "))
                .map(s -> "(?=.*" + Pattern.quote(s) + ")")
                .collect(() -> stringBuilder, StringBuilder::append, StringBuilder::append)
                .append(".*")
                .toString();
        var regex = Pattern.compile(pattern, CASE_INSENSITIVE);
        filteredViewItems = recyclerViewItems.stream()
                .filter(g -> regex.matcher(g.path).matches())
                .collect(Collectors.toList());
        notifyDataSetChanged();
    }

    public void clearItems() {
        if (filteredViewItems.isEmpty()) return;
        int itemsCount = filteredViewItems.size();
        filteredViewItems.clear();
        notifyItemRangeRemoved(0, itemsCount);
    }

    @NonNull
    @Override
    public RecyclerView.ViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int position) {
        return filteredViewItems.get(position).onCreateViewHolder(parent);
    }

    @Override
    public int getItemViewType(int position) {
        return position;
    }

    @Override
    public void onBindViewHolder(@NonNull RecyclerView.ViewHolder holder, int position) {
        filteredViewItems.get(position).onBindViewHolder(holder, this);
    }

    @Override
    public int getItemCount() {
        return filteredViewItems.size();
    }
}
