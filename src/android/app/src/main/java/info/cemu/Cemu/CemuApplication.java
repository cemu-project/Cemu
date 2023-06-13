package info.cemu.Cemu;

import android.app.Application;
import android.util.DisplayMetrics;

public class CemuApplication extends Application {
    @Override
    public void onCreate() {
        super.onCreate();
        DisplayMetrics displayMetrics = getResources().getDisplayMetrics();
        NativeLibrary.setDPI(displayMetrics.density);
        NativeLibrary.initializeActiveSettings(getExternalFilesDir(null).getAbsoluteFile().toString(), getCacheDir().toString());
        NativeLibrary.initializeEmulation();
    }
}
