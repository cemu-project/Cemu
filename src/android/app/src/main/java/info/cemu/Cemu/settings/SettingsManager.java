package info.cemu.Cemu.settings;

import android.content.Context;
import android.content.SharedPreferences;

public class SettingsManager {
    public static final String PREFERENCES_NAME = "CEMU_PREFERENCES";
    public static final String GAMES_PATH_KEY = "GAME_PATHS_KEY";
    private final SharedPreferences sharedPreferences;

    public SettingsManager(Context context) {
        sharedPreferences = context.getSharedPreferences(PREFERENCES_NAME, Context.MODE_PRIVATE);
    }

    public String getGamesPath() {
        return sharedPreferences.getString(GAMES_PATH_KEY, null);
    }

    public void setGamesPath(String gamesPath) {
        sharedPreferences.edit().putString(GAMES_PATH_KEY, gamesPath).apply();
    }
}
