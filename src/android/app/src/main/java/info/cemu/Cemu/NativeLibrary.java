package info.cemu.Cemu;

import android.view.Surface;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.Objects;

public class NativeLibrary {
    static {
        System.loadLibrary("CemuAndroid");
    }

    public static native void setDPI(float dpi);

    public static native void setSurface(Surface surface, boolean isMainCanvas);

    public static native void clearSurface(boolean isMainCanvas);

    public static native void setSurfaceSize(int width, int height, boolean isMainCanvas);

    public static native void initializerRenderer(Surface surface);

    public static native void startGame(long titleId);

    public static native void setReplaceTVWithPadView(boolean swapped);

    public static native void recreateRenderSurface(boolean isMainCanvas);

    public interface GameTitleLoadedCallback {
        void onGameTitleLoaded(long titleId, String title, int[] colors, int width, int height);
    }

    public static native void setGameTitleLoadedCallback(GameTitleLoadedCallback gameTitleLoadedCallback);

    public static native void reloadGameTitles();

    public static native void initializeActiveSettings(String dataPath, String cachePath);

    public static native void initializeEmulation();

    public static native void addGamesPath(String uri);

    public static native void removeGamesPath(String uri);

    public static native ArrayList<String> getGamesPaths();

    public static native void onNativeKey(String deviceDescriptor, String deviceName, int key, boolean isPressed);

    public static native void onNativeAxis(String deviceDescriptor, String deviceName, int axis, float value);

    public static final int VPAD_BUTTON_NONE = 0;
    public static final int VPAD_BUTTON_A = 1;
    public static final int VPAD_BUTTON_B = 2;
    public static final int VPAD_BUTTON_X = 3;
    public static final int VPAD_BUTTON_Y = 4;
    public static final int VPAD_BUTTON_L = 5;
    public static final int VPAD_BUTTON_R = 6;
    public static final int VPAD_BUTTON_ZL = 7;
    public static final int VPAD_BUTTON_ZR = 8;
    public static final int VPAD_BUTTON_PLUS = 9;
    public static final int VPAD_BUTTON_MINUS = 10;
    public static final int VPAD_BUTTON_UP = 11;
    public static final int VPAD_BUTTON_DOWN = 12;
    public static final int VPAD_BUTTON_LEFT = 13;
    public static final int VPAD_BUTTON_RIGHT = 14;
    public static final int VPAD_BUTTON_STICKL = 15;
    public static final int VPAD_BUTTON_STICKR = 16;
    public static final int VPAD_BUTTON_STICKL_UP = 17;
    public static final int VPAD_BUTTON_STICKL_DOWN = 18;
    public static final int VPAD_BUTTON_STICKL_LEFT = 19;
    public static final int VPAD_BUTTON_STICKL_RIGHT = 20;
    public static final int VPAD_BUTTON_STICKR_UP = 21;
    public static final int VPAD_BUTTON_STICKR_DOWN = 22;
    public static final int VPAD_BUTTON_STICKR_LEFT = 23;
    public static final int VPAD_BUTTON_STICKR_RIGHT = 24;
    public static final int VPAD_BUTTON_MIC = 25;
    public static final int VPAD_BUTTON_SCREEN = 26;
    public static final int VPAD_BUTTON_HOME = 27;
    public static final int VPAD_BUTTON_MAX = 28;

    public static final int PRO_BUTTON_NONE = 0;
    public static final int PRO_BUTTON_A = 1;
    public static final int PRO_BUTTON_B = 2;
    public static final int PRO_BUTTON_X = 3;
    public static final int PRO_BUTTON_Y = 4;
    public static final int PRO_BUTTON_L = 5;
    public static final int PRO_BUTTON_R = 6;
    public static final int PRO_BUTTON_ZL = 7;
    public static final int PRO_BUTTON_ZR = 8;
    public static final int PRO_BUTTON_PLUS = 9;
    public static final int PRO_BUTTON_MINUS = 10;
    public static final int PRO_BUTTON_HOME = 11;
    public static final int PRO_BUTTON_UP = 12;
    public static final int PRO_BUTTON_DOWN = 13;
    public static final int PRO_BUTTON_LEFT = 14;
    public static final int PRO_BUTTON_RIGHT = 15;
    public static final int PRO_BUTTON_STICKL = 16;
    public static final int PRO_BUTTON_STICKR = 17;
    public static final int PRO_BUTTON_STICKL_UP = 18;
    public static final int PRO_BUTTON_STICKL_DOWN = 19;
    public static final int PRO_BUTTON_STICKL_LEFT = 20;
    public static final int PRO_BUTTON_STICKL_RIGHT = 21;
    public static final int PRO_BUTTON_STICKR_UP = 22;
    public static final int PRO_BUTTON_STICKR_DOWN = 23;
    public static final int PRO_BUTTON_STICKR_LEFT = 24;
    public static final int PRO_BUTTON_STICKR_RIGHT = 25;
    public static final int PRO_BUTTON_MAX = 26;

    public static final int CLASSIC_BUTTON_NONE = 0;
    public static final int CLASSIC_BUTTON_A = 1;
    public static final int CLASSIC_BUTTON_B = 2;
    public static final int CLASSIC_BUTTON_X = 3;
    public static final int CLASSIC_BUTTON_Y = 4;
    public static final int CLASSIC_BUTTON_L = 5;
    public static final int CLASSIC_BUTTON_R = 6;
    public static final int CLASSIC_BUTTON_ZL = 7;
    public static final int CLASSIC_BUTTON_ZR = 8;
    public static final int CLASSIC_BUTTON_PLUS = 9;
    public static final int CLASSIC_BUTTON_MINUS = 10;
    public static final int CLASSIC_BUTTON_HOME = 11;
    public static final int CLASSIC_BUTTON_UP = 12;
    public static final int CLASSIC_BUTTON_DOWN = 13;
    public static final int CLASSIC_BUTTON_LEFT = 14;
    public static final int CLASSIC_BUTTON_RIGHT = 15;
    public static final int CLASSIC_BUTTON_STICKL_UP = 16;
    public static final int CLASSIC_BUTTON_STICKL_DOWN = 17;
    public static final int CLASSIC_BUTTON_STICKL_LEFT = 18;
    public static final int CLASSIC_BUTTON_STICKL_RIGHT = 19;
    public static final int CLASSIC_BUTTON_STICKR_UP = 20;
    public static final int CLASSIC_BUTTON_STICKR_DOWN = 21;
    public static final int CLASSIC_BUTTON_STICKR_LEFT = 22;
    public static final int CLASSIC_BUTTON_STICKR_RIGHT = 23;
    public static final int CLASSIC_BUTTON_MAX = 24;

    public static final int WIIMOTE_BUTTON_NONE = 0;
    public static final int WIIMOTE_BUTTON_A = 1;
    public static final int WIIMOTE_BUTTON_B = 2;
    public static final int WIIMOTE_BUTTON_1 = 3;
    public static final int WIIMOTE_BUTTON_2 = 4;
    public static final int WIIMOTE_BUTTON_NUNCHUCK_Z = 5;
    public static final int WIIMOTE_BUTTON_NUNCHUCK_C = 6;
    public static final int WIIMOTE_BUTTON_PLUS = 7;
    public static final int WIIMOTE_BUTTON_MINUS = 8;
    public static final int WIIMOTE_BUTTON_UP = 9;
    public static final int WIIMOTE_BUTTON_DOWN = 10;
    public static final int WIIMOTE_BUTTON_LEFT = 11;
    public static final int WIIMOTE_BUTTON_RIGHT = 12;
    public static final int WIIMOTE_BUTTON_NUNCHUCK_UP = 13;
    public static final int WIIMOTE_BUTTON_NUNCHUCK_DOWN = 14;
    public static final int WIIMOTE_BUTTON_NUNCHUCK_LEFT = 15;
    public static final int WIIMOTE_BUTTON_NUNCHUCK_RIGHT = 16;
    public static final int WIIMOTE_BUTTON_HOME = 17;
    public static final int WIIMOTE_BUTTON_MAX = 18;

    public static final int EMULATED_CONTROLLER_TYPE_VPAD = 0;
    public static final int EMULATED_CONTROLLER_TYPE_PRO = 1;
    public static final int EMULATED_CONTROLLER_TYPE_CLASSIC = 2;
    public static final int EMULATED_CONTROLLER_TYPE_WIIMOTE = 3;
    public static final int EMULATED_CONTROLLER_TYPE_DISABLED = -1;

    public static final int DPAD_UP = 34;
    public static final int DPAD_DOWN = 35;
    public static final int DPAD_LEFT = 36;
    public static final int DPAD_RIGHT = 37;
    public static final int AXIS_X_POS = 38;
    public static final int AXIS_Y_POS = 39;
    public static final int ROTATION_X_POS = 40;
    public static final int ROTATION_Y_POS = 41;
    public static final int TRIGGER_X_POS = 42;
    public static final int TRIGGER_Y_POS = 43;
    public static final int AXIS_X_NEG = 44;
    public static final int AXIS_Y_NEG = 45;
    public static final int ROTATION_X_NEG = 46;
    public static final int ROTATION_Y_NEG = 47;

    public static final int MAX_CONTROLLERS = 8;
    public static final int MAX_VPAD_CONTROLLERS = 2;
    public static final int MAX_WPAD_CONTROLLERS = 7;

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

    public static final int VSYNC_MODE_OFF = 0;
    public static final int VSYNC_MODE_DOUBLE_BUFFERING = 1;
    public static final int VSYNC_MODE_TRIPLE_BUFFERING = 2;

    public static native int getVSyncMode();

    public static native void setVSyncMode(int vsyncMode);

    public static native boolean getAccurateBarriers();

    public static native void setAccurateBarriers(boolean enabled);

    public static native boolean getAudioDeviceEnabled(boolean tv);

    public static native void setAudioDeviceEnabled(boolean enabled, boolean tv);

    public static final int AUDIO_CHANNELS_MONO = 0;
    public static final int AUDIO_CHANNELS_STEREO = 1;
    public static final int AUDIO_CHANNELS_SURROUND = 2;

    public static native void setAudioDeviceChannels(int channels, boolean tv);

    public static native int getAudioDeviceChannels(boolean tv);

    public static final int AUDIO_MIN_VOLUME = 0;
    public static final int AUDIO_MAX_VOLUME = 100;

    public static native void setAudioDeviceVolume(int volume, boolean tv);

    public static native int getAudioDeviceVolume(boolean tv);

    public record GraphicPackBasicInfo(long id, String virtualPath, ArrayList<Long> titleIds) {
    }

    public static native ArrayList<Long> getInstalledGamesTitleIds();

    public static native ArrayList<GraphicPackBasicInfo> getGraphicPackBasicInfos();

    public static class GraphicPackPreset {
        private final long graphicPackId;
        private final String category;
        private final ArrayList<String> presets;
        private String activePreset;

        @Override
        public int hashCode() {
            return Objects.hash(graphicPackId, category, presets, activePreset);
        }

        @Override
        public boolean equals(Object object) {
            if (object == null) return false;
            if (object == this) return true;
            if (object instanceof GraphicPackPreset preset)
                return this.hashCode() == preset.hashCode();
            return false;
        }

        public GraphicPackPreset(long graphicPackId, String category, ArrayList<String> presets, String activePreset) {
            this.graphicPackId = graphicPackId;
            this.category = category;
            this.presets = presets;
            this.activePreset = activePreset;
        }

        public String getActivePreset() {
            return activePreset;
        }

        public void setActivePreset(String activePreset) {
            if (presets.stream().noneMatch(s -> s.equals(activePreset)))
                throw new IllegalArgumentException("Trying to set an invalid preset: " + activePreset);
            setGraphicPackActivePreset(graphicPackId, category, activePreset);
            this.activePreset = activePreset;
        }

        public String getCategory() {
            return category;
        }

        public ArrayList<String> getPresets() {
            return presets;
        }
    }

    public static class GraphicPack {
        public final long id;
        protected boolean active;
        public final String name;
        public final String description;
        public List<GraphicPackPreset> presets;

        public GraphicPack(long id, boolean active, String name, String description, ArrayList<GraphicPackPreset> presets) {
            this.id = id;
            this.active = active;
            this.name = name;
            this.description = description;
            this.presets = presets;
        }

        public boolean isActive() {
            return active;
        }

        public void reloadPresets() {
            presets = NativeLibrary.getGraphicPackPresets(id);
        }

        public void setActive(boolean active) {
            this.active = active;
            setGraphicPackActive(id, active);
        }
    }

    public static native void refreshGraphicPacks();

    public static native GraphicPack getGraphicPack(long id);

    public static native void setGraphicPackActive(long id, boolean active);

    public static native void setGraphicPackActivePreset(long id, String category, String preset);

    public static native ArrayList<GraphicPackPreset> getGraphicPackPresets(long id);

    public static native void onOverlayButton(int controllerIndex, int mappingId, boolean value);

    public static native void onOverlayAxis(int controllerIndex, int mappingId, float value);

    public static final int OVERLAY_SCREEN_POSITION_DISABLED = 0;
    public static final int OVERLAY_SCREEN_POSITION_TOP_LEFT = 1;
    public static final int OVERLAY_SCREEN_POSITION_TOP_CENTER = 2;
    public static final int OVERLAY_SCREEN_POSITION_TOP_RIGHT = 3;
    public static final int OVERLAY_SCREEN_POSITION_BOTTOM_LEFT = 4;
    public static final int OVERLAY_SCREEN_POSITION_BOTTOM_CENTER = 5;
    public static final int OVERLAY_SCREEN_POSITION_BOTTOM_RIGHT = 6;

    public static native int getOverlayPosition();

    public static native void setOverlayPosition(int position);

    public static native boolean isOverlayFPSEnabled();

    public static native void setOverlayFPSEnabled(boolean enabled);

    public static native boolean isOverlayDrawCallsPerFrameEnabled();

    public static native void setOverlayDrawCallsPerFrameEnabled(boolean enabled);

    public static native boolean isOverlayCPUUsageEnabled();

    public static native void setOverlayCPUUsageEnabled(boolean enabled);

    public static native boolean isOverlayCPUPerCoreUsageEnabled();

    public static native void setOverlayCPUPerCoreUsageEnabled(boolean enabled);

    public static native boolean isOverlayRAMUsageEnabled();

    public static native void setOverlayRAMUsageEnabled(boolean enabled);

    public static native boolean isOverlayDebugEnabled();

    public static native void setOverlayDebugEnabled(boolean enabled);

    public static native int getNotificationsPosition();

    public static native void setNotificationsPosition(int position);

    public static native boolean isNotificationControllerProfilesEnabled();

    public static native void setNotificationControllerProfilesEnabled(boolean enabled);

    public static native boolean isNotificationShaderCompilerEnabled();

    public static native void setNotificationShaderCompilerEnabled(boolean enabled);

    public static native boolean isNotificationFriendListEnabled();

    public static native void setNotificationFriendListEnabled(boolean enabled);

    public static native void onTouchDown(int x, int y, boolean isTV);

    public static native void onTouchMove(int x, int y, boolean isTV);

    public static native void onTouchUp(int x, int y, boolean isTV);

    public static native void onMotion(long timestamp, float gyroX, float gyroY, float gyroZ, float accelX, float accelY, float accelZ);

    public static native void setMotionEnabled(boolean motionEnabled);
}
