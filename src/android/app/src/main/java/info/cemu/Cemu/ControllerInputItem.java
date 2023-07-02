package info.cemu.Cemu;

public class ControllerInputItem implements ControllerItem {
    private final int nameResourceId;
    private final int id;
    private String boundInput;

    public ControllerInputItem(int nameResourceId, int id, String boundInput) {
        this.nameResourceId = nameResourceId;
        this.id = id;
        this.boundInput = boundInput;
    }

    @Override
    public int getType() {
        return TYPE_INPUT;
    }

    public int getNameResourceId() {
        return nameResourceId;
    }

    public int getId() {
        return id;
    }

    public String getBoundInput() {
        return boundInput;
    }

    public void setBoundInput(String boundInput) {
        this.boundInput = boundInput;
    }
}
