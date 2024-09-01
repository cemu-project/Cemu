package info.cemu.Cemu.settings.graphicpacks;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;

import info.cemu.Cemu.R;
import info.cemu.Cemu.guibasecomponents.RecyclerViewItem;

public class GraphicPackItemRecyclerViewItem implements RecyclerViewItem {

    private static class HeaderViewHolder extends RecyclerView.ViewHolder {
        TextView title;
        ImageView icon;

        public HeaderViewHolder(View itemView) {
            super(itemView);
            title = itemView.findViewById(R.id.graphic_pack_text);
            icon = itemView.findViewById(R.id.graphic_pack_icon);
        }
    }

    public interface OnClickCallback {
        void onClick();
    }

    public enum GraphicPackItemType {
        SECTION,
        GRAPHIC_PACK,
    }

    private final String text;
    public final boolean hasInstalledTitleId;
    private final GraphicPackItemType graphicPackItemType;
    private final OnClickCallback onClickCallback;

    public GraphicPackItemRecyclerViewItem(String text, boolean hasInstalledTitleId, GraphicPackItemType graphicPackItemType, OnClickCallback onClickCallback) {
        this.text = text;
        this.hasInstalledTitleId = hasInstalledTitleId;
        this.graphicPackItemType = graphicPackItemType;
        this.onClickCallback = onClickCallback;
    }

    @Override
    public RecyclerView.ViewHolder onCreateViewHolder(ViewGroup parent) {
        return new HeaderViewHolder(LayoutInflater.from(parent.getContext()).inflate(R.layout.layout_graphic_pack_item, parent, false));
    }

    @Override
    public void onBindViewHolder(RecyclerView.ViewHolder viewHolder, RecyclerView.Adapter<RecyclerView.ViewHolder> adapter) {
        HeaderViewHolder headerViewHolder = (HeaderViewHolder) viewHolder;
        headerViewHolder.itemView.setOnClickListener(v -> onClickCallback.onClick());
        headerViewHolder.title.setText(text);
        if (graphicPackItemType == GraphicPackItemType.SECTION)
            headerViewHolder.icon.setImageResource(R.drawable.ic_lists);
        else if (graphicPackItemType == GraphicPackItemType.GRAPHIC_PACK)
            headerViewHolder.icon.setImageResource(R.drawable.ic_package_2);
    }
}
