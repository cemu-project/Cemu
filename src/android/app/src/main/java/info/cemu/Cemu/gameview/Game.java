package info.cemu.Cemu.gameview;

import android.graphics.Bitmap;

public class Game {
    Long titleId;
    String title;
    Bitmap icon;

    public Game(Long titleId, String title, Bitmap icon) {
        this.titleId = titleId;
        this.title = title;
        this.icon = icon;
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
