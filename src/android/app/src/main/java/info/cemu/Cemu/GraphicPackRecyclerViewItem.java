package info.cemu.Cemu;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import com.google.android.material.checkbox.MaterialCheckBox;
import com.google.android.material.dialog.MaterialAlertDialogBuilder;

public class GraphicPackRecyclerViewItem implements RecyclerViewItem {
    private static class GraphicPackViewHolder extends RecyclerView.ViewHolder {
        TextView name;
        MaterialCheckBox enableCheckBox;
        TextView description;

        public GraphicPackViewHolder(View itemView) {
            super(itemView);
            name = itemView.findViewById(R.id.graphic_pack_name);
            enableCheckBox = itemView.findViewById(R.id.graphic_pack_enable_checkbox);
            description = itemView.findViewById(R.id.graphic_pack_description);
        }
    }


    private final NativeLibrary.GraphicPack graphicPack;

    public GraphicPackRecyclerViewItem(NativeLibrary.GraphicPack graphicPack) {
        this.graphicPack = graphicPack;
    }

    @Override
    public RecyclerView.ViewHolder onCreateViewHolder(ViewGroup parent) {
        return new GraphicPackViewHolder(LayoutInflater.from(parent.getContext()).inflate(R.layout.layout_graphic_pack, parent, false));
    }

    @Override
    public void onBindViewHolder(RecyclerView.ViewHolder viewHolder, RecyclerView.Adapter<RecyclerView.ViewHolder> recyclerViewAdapter) {
        var graphicPackViewHolder = (GraphicPackViewHolder) viewHolder;
        graphicPackViewHolder.name.setText(graphicPack.name);
        if (graphicPack.description != null) {
            graphicPackViewHolder.description.setText(graphicPack.description);
        } else {
            graphicPackViewHolder.description.setText(R.string.graphic_pack_no_description);
        }
        graphicPackViewHolder.enableCheckBox.setChecked(graphicPack.isActive());
        graphicPackViewHolder.enableCheckBox.addOnCheckedStateChangedListener((materialCheckBox, i) -> graphicPack.setActive(materialCheckBox.isChecked()));
    }
}
