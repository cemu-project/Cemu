package info.cemu.Cemu;

import android.app.Application;
import android.os.FileUtils;
import android.util.DisplayMetrics;

import java.util.ArrayList;

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
