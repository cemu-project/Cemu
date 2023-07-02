package info.cemu.Cemu;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import java.util.ArrayList;
import java.util.List;

public class ButtonListAdapter extends RecyclerView.Adapter<RecyclerView.ViewHolder> {

    public interface OnButtonClickListener {
        void onButtonClicked();
    }

    private static class ButtonInfo {
        public ButtonInfo(String text, String description, OnButtonClickListener onButtonClickListener) {
            this.text = text;
            this.description = description;
            this.onButtonClickListener = onButtonClickListener;
        }

        String text;
        String description;
        OnButtonClickListener onButtonClickListener;
    }

    private final List<ButtonInfo> buttonInfoList = new ArrayList<>();

    public void addButtonInfo(String text, String description, OnButtonClickListener onButtonClickListener) {
        buttonInfoList.add(new ButtonInfo(text, description, onButtonClickListener));
        notifyItemInserted(buttonInfoList.size());
    }

    @NonNull
    @Override
    public RecyclerView.ViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        return new ButtonViewHolder(LayoutInflater.from(parent.getContext()).inflate(R.layout.layout_button, parent, false));
    }

    @Override
    public void onBindViewHolder(@NonNull RecyclerView.ViewHolder holder, int position) {
        ((ButtonViewHolder) holder).bind(buttonInfoList.get(position));
        holder.itemView.setOnClickListener(view -> {
            ButtonInfo buttonInfo = buttonInfoList.get(holder.getAdapterPosition());
            if (buttonInfo.onButtonClickListener != null) {
                buttonInfo.onButtonClickListener.onButtonClicked();
            }
        });
    }

    @Override
    public int getItemCount() {
        return buttonInfoList.size();
    }

    private static class ButtonViewHolder extends RecyclerView.ViewHolder {
        TextView text;
        TextView description;

        void bind(ButtonInfo buttonInfo) {
            text.setText(buttonInfo.text);
            description.setText(buttonInfo.description);
        }

        public ButtonViewHolder(View itemView) {
            super(itemView);
            text = itemView.findViewById(R.id.button_text);
            description = itemView.findViewById(R.id.button_description);
        }
    }

}
