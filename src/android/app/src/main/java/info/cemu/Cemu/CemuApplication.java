package info.cemu.Cemu;

import android.app.Application;

public class CemuApplication extends Application {
    @Override
    public void onCreate() {
        super.onCreate();
        NativeLibrary.initializeActiveSettings(getExternalFilesDir(null).getAbsoluteFile().toString(), getCacheDir().toString());
        NativeLibrary.initializeEmulation();
    }
}
