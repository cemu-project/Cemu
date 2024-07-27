package info.cemu.Cemu.guibasecomponents;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;

import info.cemu.Cemu.R;

public class HeaderRecyclerViewItem implements RecyclerViewItem {

    private static class HeaderViewHolder extends RecyclerView.ViewHolder {
        TextView header;

        public HeaderViewHolder(View itemView) {
            super(itemView);
            header = itemView.findViewById(R.id.textview_header);
        }
    }

    private final int headerResourceIdText;

    public HeaderRecyclerViewItem(int headerResourceIdText) {
        this.headerResourceIdText = headerResourceIdText;
    }

    @Override
    public RecyclerView.ViewHolder onCreateViewHolder(ViewGroup parent) {
        return new HeaderViewHolder(LayoutInflater.from(parent.getContext()).inflate(R.layout.layout_header, parent, false));
    }

    @Override
    public void onBindViewHolder(RecyclerView.ViewHolder viewHolder, RecyclerView.Adapter<RecyclerView.ViewHolder> adapter) {
        HeaderViewHolder headerViewHolder = (HeaderViewHolder) viewHolder;
        headerViewHolder.header.setText(headerResourceIdText);
    }
}
