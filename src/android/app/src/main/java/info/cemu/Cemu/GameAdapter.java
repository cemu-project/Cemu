package info.cemu.Cemu;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;


import java.util.HashMap;
import java.util.Vector;

public class GameAdapter extends RecyclerView.Adapter<GameAdapter.ViewHolder> {
    HashMap<Long, Game> gameInfoMap;
    Vector<Long> gameTitleIds;
    private final GameTitleClickAction gameTitleClickAction;

    public interface GameTitleClickAction {
        void action(long titleId);
    }

    public GameAdapter(GameTitleClickAction gameTitleClickAction) {
        super();
        this.gameTitleClickAction = gameTitleClickAction;
        gameTitleIds = new Vector<>();
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
        Game game = gameInfoMap.get(gameTitleIds.get(position));
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
        return gameTitleIds.size();
    }

    void clear() {
        int itemCount = gameTitleIds.size();
        notifyItemRangeRemoved(0, itemCount);
        gameInfoMap.clear();
        gameTitleIds.clear();
    }

    void addGameInfo(long titleId, String title) {
        gameTitleIds.addElement(titleId);
        gameInfoMap.put(titleId, new Game(titleId, title));
        notifyItemInserted(gameTitleIds.size() - 1);
    }

    void setGameIcon(long titleId, int[] colors, int width, int height) {
        Game game = gameInfoMap.get(titleId);
        if (game != null) {
            game.setIconData(colors, width, height);
            notifyItemChanged(gameTitleIds.indexOf(titleId));
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
