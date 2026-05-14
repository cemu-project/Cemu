package info.cemu.cemu.settings.inputoverlay

import androidx.datastore.core.DataStore
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import info.cemu.cemu.common.settings.AppSettings
import info.cemu.cemu.common.settings.AppSettingsStore
import info.cemu.cemu.common.settings.InputOverlaySettings
import info.cemu.cemu.common.settings.OverlayInputConfig
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch

class InputOverlaySettingsViewModel(
    val dataStore: DataStore<AppSettings> = AppSettingsStore.dataStore
) : ViewModel() {
    val overlaySettings = dataStore.data.map { it.inputOverlaySettings }
        .stateIn(
            viewModelScope,
            SharingStarted.WhileSubscribed(5000),
            InputOverlaySettings(),
        )

    private fun updateOverlay(transform: (InputOverlaySettings) -> InputOverlaySettings) {
        viewModelScope.launch {
            dataStore.updateData {
                it.copy(
                    inputOverlaySettings = transform(it.inputOverlaySettings)
                )
            }
        }
    }

    fun setVibrateOnTouch(enabled: Boolean) = updateOverlay {
        it.copy(isVibrateOnTouchEnabled = enabled)
    }

    fun setOverlayEnabled(enabled: Boolean) = updateOverlay {
        it.copy(isOverlayEnabled = enabled)
    }

    fun setControllerIndex(index: Int) = updateOverlay {
        it.copy(controllerIndex = index)
    }

    fun setAlpha(alpha: Int) = updateOverlay {
        it.copy(alpha = alpha)
    }

    fun setInputVisibility(input: OverlayInputConfig, visible: Boolean) = updateOverlay {
        it.copy(
            inputVisibilityMap = it.inputVisibilityMap.toMutableMap().apply {
                this[input] = visible
            }
        )
    }
}