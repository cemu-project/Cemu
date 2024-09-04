package info.cemu.Cemu;

import android.app.Application;
import android.util.DisplayMetrics;

import info.cemu.Cemu.NativeLibrary;
import info.cemu.Cemu.utils.FileUtil;

public class CemuApplication extends Application {
    private static CemuApplication application;

    public static CemuApplication getApplication() {
        return application;
    }

    @Override
    public void onCreate() {
        super.onCreate();
        application = this;
        DisplayMetrics displayMetrics = getResources().getDisplayMetrics();
        NativeLibrary.setDPI(displayMetrics.density);
        NativeLibrary.initializeActiveSettings(getExternalFilesDir(null).getAbsoluteFile().toString(), getExternalFilesDir(null).getAbsoluteFile().toString());
        NativeLibrary.initializeEmulation();
    }
}
