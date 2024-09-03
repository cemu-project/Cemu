package info.cemu.Cemu.settings;

import android.content.Context;
import android.content.SharedPreferences;

public class SettingsManager {
    public static final String PREFERENCES_NAME = "CEMU_PREFERENCES";
    private final SharedPreferences sharedPreferences;

    public SettingsManager(Context context) {
        sharedPreferences = context.getSharedPreferences(PREFERENCES_NAME, Context.MODE_PRIVATE);
    }
}
