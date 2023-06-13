package info.cemu.Cemu;

import android.view.Surface;

public class NativeLibrary {
    static {
        System.loadLibrary("CemuAndroid");
    }

    public static native void setDPI(float dpi);

    public static native void setSurface(Surface surface, boolean isMainCanvas);

    public static native void setSurfaceSize(int width, int height, boolean isMainCanvas);

    public static native void initializerRenderer();

    public static native void initializeRendererSurface(boolean isMainCanvas);

    public static native void startGame(long titleId);


    public interface GameTitleLoadedCallback {
        void onGameTitleLoaded(long titleId, String title);
    }

    public static native void setGameTitleLoadedCallback(GameTitleLoadedCallback gameTitleLoadedCallback);

    public interface GameIconLoadedCallback {
        void onGameIconLoaded(long titleId, int[] colors, int width, int height);
    }

    public static native void setGameIconLoadedCallback(GameIconLoadedCallback gameIconLoadedCallback);

    public static native void requestGameIcon(long titleId);

    public static native void reloadGameTitles();

    public static native void initializeActiveSettings(String dataPath, String cachePath);

    public static native void initializeEmulation();
}
