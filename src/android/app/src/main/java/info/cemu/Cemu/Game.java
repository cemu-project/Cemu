package info.cemu.Cemu;

import android.graphics.Bitmap;

public class Game {
    Long titleId;
    String title;
    Bitmap icon;

    public Game(Long titleId, String title) {
        this.titleId = titleId;
        this.title = title;
    }

    public void setIconData(int[] colors, int width, int height) {
        icon = Bitmap.createBitmap(colors, width, height, Bitmap.Config.ARGB_8888);
    }

    public Bitmap getIcon() {
        return icon;
    }

    public Long getTitleId() {
        return titleId;
    }

    public String getTitle() {
        return title;
    }
}
