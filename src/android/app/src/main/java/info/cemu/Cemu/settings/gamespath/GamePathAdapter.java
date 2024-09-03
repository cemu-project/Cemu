package info.cemu.Cemu.settings.gamespath;

import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.DiffUtil;
import androidx.recyclerview.widget.ListAdapter;
import androidx.recyclerview.widget.RecyclerView;

import info.cemu.Cemu.R;

public class GamePathAdapter extends ListAdapter<String, GamePathAdapter.ViewHolder> {
    public interface OnRemoveGamePath {
        void onRemoveGamePath(String path);
    }

    private final static DiffUtil.ItemCallback<String> DIFF_CALLBACK = new DiffUtil.ItemCallback<>() {
        @Override
        public boolean areItemsTheSame(@NonNull String oldItem, @NonNull String newItem) {
            return oldItem.equals(newItem);
        }

        @Override
        public boolean areContentsTheSame(@NonNull String oldItem, @NonNull String newItem) {
            return oldItem.equals(newItem);
        }
    };
    private final OnRemoveGamePath onRemoveGamePath;

    public GamePathAdapter(OnRemoveGamePath onRemoveGamePath) {
        super(DIFF_CALLBACK);
        this.onRemoveGamePath = onRemoveGamePath;
    }

    @NonNull
    @Override
    public ViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        View view = LayoutInflater.from(parent.getContext()).inflate(R.layout.layout_game_path, parent, false);
        return new ViewHolder(view);
    }

    @Override
    public void onBindViewHolder(@NonNull ViewHolder holder, int position) {
        String gamePath = getItem(position);
        holder.gamePath.setText(gamePath);
        holder.gamePath.setSelected(true);
        holder.deleteButton.setOnClickListener(v -> onRemoveGamePath.onRemoveGamePath(gamePath));
    }

    public static class ViewHolder extends RecyclerView.ViewHolder {
        TextView gamePath;
        Button deleteButton;

        public ViewHolder(View itemView) {
            super(itemView);
            gamePath = itemView.findViewById(R.id.game_path_text);
            deleteButton = itemView.findViewById(R.id.remove_game_path_button);
        }
    }
}
