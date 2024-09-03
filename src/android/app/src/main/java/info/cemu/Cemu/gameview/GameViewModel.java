package info.cemu.Cemu.gameview;

import android.graphics.Bitmap;
import android.util.Log;

import androidx.lifecycle.LiveData;
import androidx.lifecycle.MutableLiveData;
import androidx.lifecycle.ViewModel;

import java.util.ArrayList;
import java.util.List;
import java.util.stream.Collectors;
import java.util.stream.Stream;

import info.cemu.Cemu.NativeLibrary;

public class GameViewModel extends ViewModel {
    private final MutableLiveData<List<Game>> gamesData;

    private final ArrayList<Game> games = new ArrayList<>();

    public LiveData<List<Game>> getGames() {
        return gamesData;
    }

    public GameViewModel() {
        this.gamesData = new MutableLiveData<>();
        NativeLibrary.setGameTitleLoadedCallback((titleId, title, colors, width, height) -> {
            Bitmap icon = null;
            if (colors != null)
                icon = Bitmap.createBitmap(colors, width, height, Bitmap.Config.ARGB_8888);
            Game game = new Game(titleId, title, icon);
            synchronized (GameViewModel.this) {
                games.add(game);
                gamesData.postValue(new ArrayList<>(games));
            }
        });
    }

    @Override
    protected void onCleared() {
        NativeLibrary.setGameTitleLoadedCallback(null);
    }

    public void refreshGames() {
        games.clear();
        gamesData.setValue(null);
        NativeLibrary.reloadGameTitles();
    }
}
