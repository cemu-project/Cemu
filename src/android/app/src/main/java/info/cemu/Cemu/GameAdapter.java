package info.cemu.Cemu;

import android.graphics.Bitmap;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Filter;
import android.widget.Filterable;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.stream.Collectors;

public class GameAdapter extends RecyclerView.Adapter<GameAdapter.ViewHolder> implements Filterable {
    private final Map<Long, Game> gameInfoMap;
    private final List<Long> gameTitleIds;
    private List<Long> filteredGameTitleIds;
    private final GameTitleClickAction gameTitleClickAction;

    @Override
    public Filter getFilter() {
        return new Filter() {
            @Override
            protected FilterResults performFiltering(CharSequence charSequence) {
                String gameTitle = charSequence.toString();
                if (gameTitle.isEmpty())
                    filteredGameTitleIds = gameTitleIds;
                else
                    filteredGameTitleIds = gameTitleIds.stream()
                            .filter(titleId -> gameInfoMap.get(titleId).title.toLowerCase().contains(gameTitle.toLowerCase()))
                            .collect(Collectors.toList());
                FilterResults filterResults = new FilterResults();
                filterResults.values = filteredGameTitleIds;
                return filterResults;
            }

            @Override
            protected void publishResults(CharSequence charSequence, FilterResults filterResults) {
                filteredGameTitleIds = (List<Long>) filterResults.values;
                notifyDataSetChanged();
            }
        };
    }


    public interface GameTitleClickAction {
        void action(long titleId);
    }

    public GameAdapter(GameTitleClickAction gameTitleClickAction) {
        super();
        this.gameTitleClickAction = gameTitleClickAction;
        gameTitleIds = new ArrayList<>();
        filteredGameTitleIds = new ArrayList<>();
        gameInfoMap = new HashMap<>();
    }

    @NonNull
    @Override
    public GameAdapter.ViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        View view = LayoutInflater.from(parent.getContext()).inflate(R.layout.layout_game, parent, false);
        return new ViewHolder(view);
    }

    @Override
    public void onBindViewHolder(@NonNull GameAdapter.ViewHolder holder, int position) {
        Game game = gameInfoMap.get(filteredGameTitleIds.get(position));
        if (game != null) {
            holder.icon.setImageBitmap(game.getIcon());
            holder.text.setText(game.getTitle());
            holder.itemView.setOnClickListener(v -> {
                long titleId = game.titleId;
                gameTitleClickAction.action(titleId);
            });
        }
    }

    @Override
    public int getItemCount() {
        return filteredGameTitleIds.size();
    }

    void clear() {
        notifyDataSetChanged();
        gameInfoMap.clear();
        gameTitleIds.clear();
        filteredGameTitleIds.clear();
    }

    void addGameInfo(long titleId, String title) {
        gameTitleIds.add(titleId);
        gameInfoMap.put(titleId, new Game(titleId, title));
        filteredGameTitleIds = gameTitleIds;
        notifyDataSetChanged();
    }

    void setGameIcon(long titleId, Bitmap icon) {
        Game game = gameInfoMap.get(titleId);
        if (game != null) {
            game.setIconData(icon);
            notifyDataSetChanged();
        }
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
