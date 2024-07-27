package info.cemu.Cemu;

import android.app.Application;
import android.util.DisplayMetrics;

import info.cemu.Cemu.NativeLibrary;
import info.cemu.Cemu.utils.FileUtil;

public class CemuApplication extends Application {
    @Override
    public void onCreate() {
        super.onCreate();
        DisplayMetrics displayMetrics = getResources().getDisplayMetrics();
        NativeLibrary.setDPI(displayMetrics.density);
        FileUtil.setCemuApplication(this);
        NativeLibrary.initializeActiveSettings(getExternalFilesDir(null).getAbsoluteFile().toString(), getExternalFilesDir(null).getAbsoluteFile().toString());
        NativeLibrary.initializeEmulation();
    }
}
