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
        NativeLibrary.setFileSystemCallbacks(new NativeLibrary.FileSystemCallbacks() {
            @Override
            public int openContentUri(String uri) {
                return FileUtil.openContentUri(getApplicationContext(), uri);
            }

            @Override
            public String[] listFiles(String uri) {
                return FileUtil.listFiles(getApplicationContext(), uri);
            }

            @Override
            public boolean isDirectory(String uri) {
                return FileUtil.isDirectory(getApplicationContext(), uri);
            }

            @Override
            public boolean isFile(String uri) {
                return FileUtil.isFile(getApplicationContext(), uri);
            }

            @Override
            public boolean exists(String uri) {
                return FileUtil.exists(getApplicationContext(), uri);
            }
        });
        NativeLibrary.initializeActiveSettings(getExternalFilesDir(null).getAbsoluteFile().toString(), getCacheDir().toString());
        NativeLibrary.initializeEmulation();
    }
}
