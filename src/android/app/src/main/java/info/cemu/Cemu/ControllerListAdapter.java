package info.cemu.Cemu;

import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

public class ControllerListAdapter extends RecyclerView.Adapter<RecyclerView.ViewHolder> {
    private List<ControllerItem> controllerItemList = new ArrayList<>();
    private final ControllerInputsDataProvider controllerInputs = new ControllerInputsDataProvider();
    private OnInputClickListener onInputClickListener;


    public void setEmulatedControllerListItems(int controllerType, Map<Integer, String> inputsMapping) {
        controllerItemList = controllerInputs.getControllerInputs(controllerType, inputsMapping);
        notifyDataSetChanged();
    }

    public void clearBoundInput(int position) {
        ControllerItem listItem = controllerItemList.get(position);
        if (listItem.getType() == ControllerItem.TYPE_INPUT) {
            ControllerInputItem controllerInputItem = (ControllerInputItem) listItem;
            controllerInputItem.setBoundInput("");
            controllerItemList.set(position, controllerInputItem);
            notifyItemChanged(position);
        }
    }

    public interface OnInputClickListener {
        void onInputClicked(int nameId, int id, int position);
    }

    void setOnInputClickListener(OnInputClickListener onInputClickListener) {
        this.onInputClickListener = onInputClickListener;
    }

    @Override
    public int getItemViewType(int position) {
        return controllerItemList.get(position).getType();
    }

    @NonNull
    @Override
    public RecyclerView.ViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        if (viewType == ControllerItem.TYPE_HEADER) {
            View view = LayoutInflater.from(parent.getContext()).inflate(R.layout.layout_controller_header, parent, false);
            return new HeaderViewHolder(view);
        } else if (viewType == ControllerItem.TYPE_INPUT) {
            View view = LayoutInflater.from(parent.getContext()).inflate(R.layout.layout_controller_input, parent, false);
            return new InputViewHolder(view);
        }
        throw new RuntimeException("Invalid viewType: " + viewType);
    }

    public void setBoundInput(int inputItemPosition, String boundInput) {
        ControllerItem listItem = controllerItemList.get(inputItemPosition);
        if (listItem.getType() == ControllerItem.TYPE_INPUT) {
            ControllerInputItem controllerInputItem = (ControllerInputItem) listItem;
            controllerInputItem.setBoundInput(boundInput);
            controllerItemList.set(inputItemPosition, controllerInputItem);
            notifyItemChanged(inputItemPosition);
        }
    }

    @Override
    public void onBindViewHolder(@NonNull RecyclerView.ViewHolder holder, int position) {
        if (holder instanceof HeaderViewHolder) {
            ((HeaderViewHolder) holder).bind((ControllerHeaderItem) controllerItemList.get(position));
        } else if (holder instanceof InputViewHolder inputViewHolder) {
            ControllerInputItem inputListItem = (ControllerInputItem) controllerItemList.get(position);
            holder.itemView.setOnClickListener(v -> {
                if (onInputClickListener != null) {
                    onInputClickListener.onInputClicked(inputListItem.getNameResourceId(), inputListItem.getId(), holder.getAdapterPosition());
                }
            });
            inputViewHolder.bind(inputListItem);
        }
    }

    @Override
    public int getItemCount() {
        return controllerItemList.size();
    }

    public static class InputViewHolder extends RecyclerView.ViewHolder {
        TextView inputName;
        TextView boundInput;

        void bind(ControllerInputItem inputListItem) {
            inputName.setText(inputListItem.getNameResourceId());
            boundInput.setText(inputListItem.getBoundInput());
        }

        public InputViewHolder(View itemView) {
            super(itemView);
            inputName = itemView.findViewById(R.id.controller_input_name);
            boundInput = itemView.findViewById(R.id.controller_button_name);
        }
    }

    public static class HeaderViewHolder extends RecyclerView.ViewHolder {
        TextView header;

        void bind(ControllerHeaderItem headerListItem) {
            header.setText(headerListItem.getHeader());
        }

        public HeaderViewHolder(View itemView) {
            super(itemView);
            header = itemView.findViewById(R.id.textview_controller_header);
        }
    }

}
