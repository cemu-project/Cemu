package info.cemu.Cemu;

public class ControllerHeaderItem implements ControllerItem {
    private String header;

    public ControllerHeaderItem(String header) {
        this.header = header;
    }

    @Override
    public int getType() {
        return TYPE_HEADER;
    }

    public String getHeader() {
        return header;
    }

    public void setHeader(String header) {
        this.header = header;
    }
}
