package info.cemu.cemu.settings.general

import android.content.Context
import androidx.datastore.core.DataStore
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import info.cemu.cemu.common.settings.AppSettings
import info.cemu.cemu.common.settings.AppSettingsStore
import info.cemu.cemu.common.settings.EmulationSettings
import info.cemu.cemu.common.settings.GamePadPosition
import info.cemu.cemu.common.settings.GuiSettings
import info.cemu.cemu.common.ui.localization.getAvailableLanguages
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch

class GeneralSettingsViewModel(
    private val dataStore: DataStore<AppSettings> = AppSettingsStore.dataStore,
) : ViewModel() {
    val languages: List<String>
    val languageToDisplayNameMap: Map<String, String>
    val emulationSettings = dataStore.data.map { it.emulationSettings }
        .stateIn(
            viewModelScope,
            SharingStarted.WhileSubscribed(5000),
            EmulationSettings(),
        )

    val guiSettings = dataStore.data.map { it.guiSettings }
        .stateIn(
            viewModelScope,
            SharingStarted.WhileSubscribed(5000),
            GuiSettings(),
        )

    init {
        val availableLanguages = getAvailableLanguages()
        languages = availableLanguages.map { it.code }
        languageToDisplayNameMap = availableLanguages.associateBy({ it.code }, { it.displayName })
    }

    fun setLanguage(language: String, context: Context) {
        info.cemu.cemu.common.ui.localization.setLanguage(language, context)

        viewModelScope.launch {
            dataStore.updateData { it.copy(guiSettings = it.guiSettings.copy(language = language)) }
        }
    }

    fun setGamePadPosition(gamePadPosition: GamePadPosition) {
        viewModelScope.launch {
            dataStore.updateData {
                it.copy(
                    emulationSettings = it.emulationSettings.copy(
                        gamePadPosition = gamePadPosition
                    )
                )
            }
        }
    }
}