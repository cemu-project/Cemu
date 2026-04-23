package info.cemu.cemu.settings.input.hotkeys

import androidx.datastore.core.DataStore
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import info.cemu.cemu.common.settings.AppSettings
import info.cemu.cemu.common.settings.AppSettingsStore
import info.cemu.cemu.common.settings.HotkeyAction
import info.cemu.cemu.common.settings.HotkeyCombo
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch

class HotkeyViewModel(
    private val dataStore: DataStore<AppSettings> = AppSettingsStore.dataStore
) : ViewModel() {
    val hotkeys = dataStore.data.map { it.hotkeySettings }.stateIn(
        viewModelScope,
        SharingStarted.WhileSubscribed(5000),
        emptyMap(),
    )

    fun setHotkeyMapping(action: HotkeyAction, combo: HotkeyCombo) = viewModelScope.launch {
        dataStore.updateData {
            it.copy(hotkeySettings = it.hotkeySettings.toMutableMap().apply { set(action, combo) })
        }
    }

    fun clearHotkeyMapping(action: HotkeyAction) = viewModelScope.launch {
        dataStore.updateData {
            it.copy(hotkeySettings = it.hotkeySettings.toMutableMap().apply { this.remove(action) })
        }
    }
}