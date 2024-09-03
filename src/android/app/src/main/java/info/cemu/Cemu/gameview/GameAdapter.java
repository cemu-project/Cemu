package info.cemu.Cemu.gameview;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.DiffUtil;
import androidx.recyclerview.widget.ListAdapter;
import androidx.recyclerview.widget.RecyclerView;

import java.util.List;
import java.util.Locale;
import java.util.stream.Collectors;

import info.cemu.Cemu.R;

public class GameAdapter extends ListAdapter<Game, GameAdapter.ViewHolder> {
    private final GameTitleClickAction gameTitleClickAction;
    private List<Game> orignalGameList;
    private String filterText;
    public static final DiffUtil.ItemCallback<Game> DIFF_CALLBACK = new DiffUtil.ItemCallback<>() {
        @Override
        public boolean areItemsTheSame(@NonNull Game oldItem, @NonNull Game newItem) {
            return oldItem.titleId.equals(newItem.titleId);
        }

        @Override
        public boolean areContentsTheSame(@NonNull Game oldItem, @NonNull Game newItem) {
            return oldItem.titleId.equals(newItem.titleId);
        }
    };

    public interface GameTitleClickAction {
        void action(long titleId);
    }

    public GameAdapter(GameTitleClickAction gameTitleClickAction) {
        super(DIFF_CALLBACK);
        this.gameTitleClickAction = gameTitleClickAction;
    }

    @Override
    public void submitList(@Nullable List<Game> list) {
        orignalGameList = list;
        if (filterText == null || filterText.isBlank() || orignalGameList == null) {
            super.submitList(orignalGameList);
            return;
        }
        super.submitList(orignalGameList.stream().filter(g -> g.title.toLowerCase(Locale.US).contains(this.filterText)).collect(Collectors.toList()));
    }

    @NonNull
    @Override
    public GameAdapter.ViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        View view = LayoutInflater.from(parent.getContext()).inflate(R.layout.layout_game, parent, false);
        return new ViewHolder(view);
    }

    @Override
    public void onBindViewHolder(@NonNull GameAdapter.ViewHolder holder, int position) {
        Game game = getItem(position);
        if (game != null) {
            holder.icon.setImageBitmap(game.getIcon());
            holder.text.setText(game.getTitle());
            holder.itemView.setOnClickListener(v -> {
                long titleId = game.titleId;
                gameTitleClickAction.action(titleId);
            });
        }
    }

    public void setFilterText(String filterText) {
        if (filterText != null) {
            filterText = filterText.toLowerCase(Locale.US);
        }
        this.filterText = filterText;
        if (filterText == null || filterText.isBlank() || orignalGameList == null) {
            super.submitList(orignalGameList);
            return;
        }
        super.submitList(orignalGameList.stream().filter(g -> g.title.toLowerCase(Locale.US).contains(this.filterText)).collect(Collectors.toList()));
    }

    public static class ViewHolder extends RecyclerView.ViewHolder {
        ImageView icon;
        TextView text;

        public ViewHolder(View itemView) {
            super(itemView);
            icon = itemView.findViewById(R.id.game_icon);
            text = itemView.findViewById(R.id.game_title);
        }
    }
}
