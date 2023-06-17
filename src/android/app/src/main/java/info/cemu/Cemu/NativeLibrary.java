package info.cemu.Cemu;

import android.view.Surface;

import java.util.ArrayList;

public class NativeLibrary {
    static {
        System.loadLibrary("CemuAndroid");
    }

    public interface FileSystemCallbacks {
        int openContentUri(String uri);

        String[] listFiles(String uri);

        boolean isDirectory(String uri);

        boolean isFile(String uri);

        boolean exists(String uri);
    }

    public static native void setFileSystemCallbacks(FileSystemCallbacks fileSystemCallbacks);

    public static native void setDPI(float dpi);

    public static native void setSurface(Surface surface, boolean isMainCanvas);

    public static native void clearSurface(boolean isMainCanvas);

    public static native void setSurfaceSize(int width, int height, boolean isMainCanvas);

    public static native void initializerRenderer();

    public static native void initializeRendererSurface(boolean isMainCanvas);

    public static native void startGame(long titleId);

    public static native void recreateRenderSurface(boolean isMainCanvas);


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

    public static native void addGamePath(String uri);
}
