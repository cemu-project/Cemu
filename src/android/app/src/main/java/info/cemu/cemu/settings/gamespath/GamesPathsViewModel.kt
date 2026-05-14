package info.cemu.cemu.settings.gamespath

import androidx.lifecycle.ViewModel
import info.cemu.cemu.nativeinterface.NativeSettings
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow

class GamesPathsViewModel : ViewModel() {
    private val _gamesPaths = MutableStateFlow(NativeSettings.getGamesPaths().toList())
    val gamesPaths = _gamesPaths.asStateFlow()

    fun addGamesPath(gamesPath: String) {
        if (!_gamesPaths.value.contains(gamesPath)) {
            _gamesPaths.value += gamesPath
            NativeSettings.addGamesPath(gamesPath)
        }
    }

    fun removeGamesPath(gamesPath: String) {
        if (_gamesPaths.value.contains(gamesPath)) {
            _gamesPaths.value -= gamesPath
            NativeSettings.removeGamesPath(gamesPath)
        }
    }
}