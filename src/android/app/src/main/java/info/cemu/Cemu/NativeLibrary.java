package info.cemu.Cemu;

import android.graphics.Bitmap;
import android.view.KeyEvent;
import android.view.Surface;

import java.util.ArrayList;
import java.util.Map;

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
        void onGameIconLoaded(long titleId);
    }

    public static native void setGameIconLoadedCallback(GameIconLoadedCallback gameIconLoadedCallback);

    public static native void requestGameIcon(long titleId);

    public static native Bitmap getGameIcon(long titleId);

    public static native void reloadGameTitles();

    public static native void initializeActiveSettings(String dataPath, String cachePath);

    public static native void initializeEmulation();

    public static native void addGamePath(String uri);

    public static native void onNativeKey(String deviceDescriptor, String deviceName, int key, boolean isPressed);

    public static native void onNativeAxis(String deviceDescriptor, String deviceName, int axis, float value);

    static final int VPAD_BUTTON_NONE = 0;
    static final int VPAD_BUTTON_A = 1;
    static final int VPAD_BUTTON_B = 2;
    static final int VPAD_BUTTON_X = 3;
    static final int VPAD_BUTTON_Y = 4;
    static final int VPAD_BUTTON_L = 5;
    static final int VPAD_BUTTON_R = 6;
    static final int VPAD_BUTTON_ZL = 7;
    static final int VPAD_BUTTON_ZR = 8;
    static final int VPAD_BUTTON_PLUS = 9;
    static final int VPAD_BUTTON_MINUS = 10;
    static final int VPAD_BUTTON_UP = 11;
    static final int VPAD_BUTTON_DOWN = 12;
    static final int VPAD_BUTTON_LEFT = 13;
    static final int VPAD_BUTTON_RIGHT = 14;
    static final int VPAD_BUTTON_STICKL = 15;
    static final int VPAD_BUTTON_STICKR = 16;
    static final int VPAD_BUTTON_STICKL_UP = 17;
    static final int VPAD_BUTTON_STICKL_DOWN = 18;
    static final int VPAD_BUTTON_STICKL_LEFT = 19;
    static final int VPAD_BUTTON_STICKL_RIGHT = 20;
    static final int VPAD_BUTTON_STICKR_UP = 21;
    static final int VPAD_BUTTON_STICKR_DOWN = 22;
    static final int VPAD_BUTTON_STICKR_LEFT = 23;
    static final int VPAD_BUTTON_STICKR_RIGHT = 24;
    static final int VPAD_BUTTON_MIC = 25;
    static final int VPAD_BUTTON_SCREEN = 26;
    static final int VPAD_BUTTON_HOME = 27;
    static final int VPAD_BUTTON_MAX = 28;

    static final int PRO_BUTTON_NONE = 0;
    static final int PRO_BUTTON_A = 1;
    static final int PRO_BUTTON_B = 2;
    static final int PRO_BUTTON_X = 3;
    static final int PRO_BUTTON_Y = 4;
    static final int PRO_BUTTON_L = 5;
    static final int PRO_BUTTON_R = 6;
    static final int PRO_BUTTON_ZL = 7;
    static final int PRO_BUTTON_ZR = 8;
    static final int PRO_BUTTON_PLUS = 9;
    static final int PRO_BUTTON_MINUS = 10;
    static final int PRO_BUTTON_HOME = 11;
    static final int PRO_BUTTON_UP = 12;
    static final int PRO_BUTTON_DOWN = 13;
    static final int PRO_BUTTON_LEFT = 14;
    static final int PRO_BUTTON_RIGHT = 15;
    static final int PRO_BUTTON_STICKL = 16;
    static final int PRO_BUTTON_STICKR = 17;
    static final int PRO_BUTTON_STICKL_UP = 18;
    static final int PRO_BUTTON_STICKL_DOWN = 19;
    static final int PRO_BUTTON_STICKL_LEFT = 20;
    static final int PRO_BUTTON_STICKL_RIGHT = 21;
    static final int PRO_BUTTON_STICKR_UP = 22;
    static final int PRO_BUTTON_STICKR_DOWN = 23;
    static final int PRO_BUTTON_STICKR_LEFT = 24;
    static final int PRO_BUTTON_STICKR_RIGHT = 25;
    static final int PRO_BUTTON_MAX = 26;

    static final int CLASSIC_BUTTON_NONE = 0;
    static final int CLASSIC_BUTTON_A = 1;
    static final int CLASSIC_BUTTON_B = 2;
    static final int CLASSIC_BUTTON_X = 3;
    static final int CLASSIC_BUTTON_Y = 4;
    static final int CLASSIC_BUTTON_L = 5;
    static final int CLASSIC_BUTTON_R = 6;
    static final int CLASSIC_BUTTON_ZL = 7;
    static final int CLASSIC_BUTTON_ZR = 8;
    static final int CLASSIC_BUTTON_PLUS = 9;
    static final int CLASSIC_BUTTON_MINUS = 10;
    static final int CLASSIC_BUTTON_HOME = 11;
    static final int CLASSIC_BUTTON_UP = 12;
    static final int CLASSIC_BUTTON_DOWN = 13;
    static final int CLASSIC_BUTTON_LEFT = 14;
    static final int CLASSIC_BUTTON_RIGHT = 15;
    static final int CLASSIC_BUTTON_STICKL_UP = 16;
    static final int CLASSIC_BUTTON_STICKL_DOWN = 17;
    static final int CLASSIC_BUTTON_STICKL_LEFT = 18;
    static final int CLASSIC_BUTTON_STICKL_RIGHT = 19;
    static final int CLASSIC_BUTTON_STICKR_UP = 20;
    static final int CLASSIC_BUTTON_STICKR_DOWN = 21;
    static final int CLASSIC_BUTTON_STICKR_LEFT = 22;
    static final int CLASSIC_BUTTON_STICKR_RIGHT = 23;
    static final int CLASSIC_BUTTON_MAX = 24;

    static final int WIIMOTE_BUTTON_NONE = 0;
    static final int WIIMOTE_BUTTON_A = 1;
    static final int WIIMOTE_BUTTON_B = 2;
    static final int WIIMOTE_BUTTON_1 = 3;
    static final int WIIMOTE_BUTTON_2 = 4;
    static final int WIIMOTE_BUTTON_NUNCHUCK_Z = 5;
    static final int WIIMOTE_BUTTON_NUNCHUCK_C = 6;
    static final int WIIMOTE_BUTTON_PLUS = 7;
    static final int WIIMOTE_BUTTON_MINUS = 8;
    static final int WIIMOTE_BUTTON_UP = 9;
    static final int WIIMOTE_BUTTON_DOWN = 10;
    static final int WIIMOTE_BUTTON_LEFT = 11;
    static final int WIIMOTE_BUTTON_RIGHT = 12;
    static final int WIIMOTE_BUTTON_NUNCHUCK_UP = 13;
    static final int WIIMOTE_BUTTON_NUNCHUCK_DOWN = 14;
    static final int WIIMOTE_BUTTON_NUNCHUCK_LEFT = 15;
    static final int WIIMOTE_BUTTON_NUNCHUCK_RIGHT = 16;
    static final int WIIMOTE_BUTTON_HOME = 17;
    static final int WIIMOTE_BUTTON_MAX = 18;

    static final int EMULATED_CONTROLLER_TYPE_VPAD = 0;
    static final int EMULATED_CONTROLLER_TYPE_PRO = 1;
    static final int EMULATED_CONTROLLER_TYPE_CLASSIC = 2;
    static final int EMULATED_CONTROLLER_TYPE_WIIMOTE = 3;
    static final int EMULATED_CONTROLLER_TYPE_DISABLED = -1;

    static final int DPAD_UP = 34;
    static final int DPAD_DOWN = 35;
    static final int DPAD_LEFT = 36;
    static final int DPAD_RIGHT = 37;
    static final int AXIS_X_POS = 38;
    static final int AXIS_Y_POS = 39;
    static final int ROTATION_X_POS = 40;
    static final int ROTATION_Y_POS = 41;
    static final int TRIGGER_X_POS = 42;
    static final int TRIGGER_Y_POS = 43;
    static final int AXIS_X_NEG = 44;
    static final int AXIS_Y_NEG = 45;
    static final int ROTATION_X_NEG = 46;
    static final int ROTATION_Y_NEG = 47;

    static final int MAX_CONTROLLERS = 8;
    static final int MAX_VPAD_CONTROLLERS = 2;
    static final int MAX_WPAD_CONTROLLERS = 7;

    public static int controllerTypeToResourceNameId(int type) {
        return switch (type) {
            case EMULATED_CONTROLLER_TYPE_DISABLED -> R.string.disabled;
            case EMULATED_CONTROLLER_TYPE_VPAD -> R.string.vpad_controller;
            case EMULATED_CONTROLLER_TYPE_PRO -> R.string.pro_controller;
            case EMULATED_CONTROLLER_TYPE_WIIMOTE -> R.string.wiimote_controller;
            case EMULATED_CONTROLLER_TYPE_CLASSIC -> R.string.classic_controller;
            default -> throw new RuntimeException("Invalid controller type: " + type);
        };
    }

    public static native void setControllerType(int index, int emulatedControllerType);

    public static native boolean isControllerDisabled(int index);

    public static native int getControllerType(int index);

    public static native int getWPADControllersCount();

    public static native int getVPADControllersCount();

    public static native void setControllerMapping(String deviceDescriptor, String deviceName, int index, int mappingId, int buttonId);

    public static native void clearControllerMapping(int index, int mappingId);

    public static native String getControllerMapping(int index, int mappingId);

    public static native Map<Integer, String> getControllerMappings(int index);

    public static native void onKeyEvent(String deviceDescriptor, String deviceName, int keyCode, boolean isPressed);

    public static native void onAxisEvent(String deviceDescriptor, String deviceName, int axisCode, float value);

    public static native boolean getAsyncShaderCompile();

    public static native void setAsyncShaderCompile(boolean enabled);

    static final int VSYNC_MODE_OFF = 0;
    static final int VSYNC_MODE_DOUBLE_BUFFERING = 1;
    static final int VSYNC_MODE_TRIPLE_BUFFERING = 2;

    public static int vsyncModeToResourceNameId(int vsyncMode) {
        return switch (vsyncMode) {
            case VSYNC_MODE_OFF -> R.string.off;
            case VSYNC_MODE_DOUBLE_BUFFERING -> R.string.double_buffering;
            case VSYNC_MODE_TRIPLE_BUFFERING -> R.string.triple_buffering;
            default -> throw new RuntimeException("Invalid vsync mode: " + vsyncMode);
        };
    }

    public static native int getVSyncMode();

    public static native void setVSyncMode(int vsyncMode);

    public static native boolean getAccurateBarriers();

    public static native void setAccurateBarriers(boolean enabled);

    public static native boolean getAudioDeviceEnabled(boolean tv);

    public static native void setAudioDeviceEnabled(boolean enabled, boolean tv);

    static final int AUDIO_CHANNELS_MONO = 0;
    static final int AUDIO_CHANNELS_STEREO = 1;
    static final int AUDIO_CHANNELS_SURROUND = 2;

    public static int channelsToResourceNameId(int channels) {
        return switch (channels) {
            case AUDIO_CHANNELS_MONO -> R.string.mono;
            case AUDIO_CHANNELS_STEREO -> R.string.stereo;
            case AUDIO_CHANNELS_SURROUND -> R.string.surround;
            default -> throw new RuntimeException("Invalid channels type: " + channels);
        };
    }

    public static native void setAudioDeviceChannels(int channels, boolean tv);

    public static native int getAudioDeviceChannels(boolean tv);

    static final int AUDIO_MIN_VOLUME = 0;
    static final int AUDIO_MAX_VOLUME = 100;

    public static native void setAudioDeviceVolume(int volume, boolean tv);

    public static native int getAudioDeviceVolume(boolean tv);

}
