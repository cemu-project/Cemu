package info.cemu.cemu.games

import androidx.lifecycle.ViewModel
import info.cemu.cemu.nativeinterface.NativeGameTitles.Game

class GameViewModel(var game: Game? = null) : ViewModel()
