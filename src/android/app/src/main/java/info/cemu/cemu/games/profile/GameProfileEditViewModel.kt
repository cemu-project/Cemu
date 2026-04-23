package info.cemu.cemu.games.profile

import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.viewModelScope
import androidx.lifecycle.viewmodel.CreationExtras
import androidx.lifecycle.viewmodel.initializer
import androidx.lifecycle.viewmodel.viewModelFactory
import info.cemu.cemu.common.customdrivers.parseInstalledDrivers
import info.cemu.cemu.nativeinterface.NativeGameTitles
import info.cemu.cemu.nativeinterface.NativeGameTitles.DriverSettingMode
import info.cemu.cemu.nativeinterface.NativeGameTitles.Game
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch

data class CustomDriverData(
    val customPath: String,
    val name: String,
)

data class DriverSettingChoice(
    val mode: Int,
    val customDriverData: CustomDriverData? = null,
)

class GameProfileEditViewModel(val game: Game) : ViewModel() {
    private val _selectedDriverSetting =
        MutableStateFlow(DriverSettingChoice(DriverSettingMode.GLOBAL))
    val selectedDriverSetting = _selectedDriverSetting.asStateFlow()

    fun setSelectedDriverSetting(driverSetting: DriverSettingChoice) {
        if (driverSetting.mode != DriverSettingMode.GLOBAL
            && driverSetting.mode != DriverSettingMode.SYSTEM
            && !driverSettingChoices.value.any { it.customDriverData != null && it.customDriverData.customPath == driverSetting.customDriverData?.customPath }
        ) {
            return
        }

        val nativeDriverSetting = NativeGameTitles.DriverSetting(
            driverSetting.mode,
            driverSetting.customDriverData?.customPath
        )

        NativeGameTitles.setDriverSettingForTitle(
            game.titleId,
            nativeDriverSetting
        )

        _selectedDriverSetting.value = driverSetting
    }

    private suspend fun getDriverSettingsChoices(): List<DriverSettingChoice> {
        val globalDriverSetting = DriverSettingChoice(DriverSettingMode.GLOBAL)

        val systemDriverSetting = DriverSettingChoice(DriverSettingMode.SYSTEM)

        val installedDrivers = parseInstalledDrivers().map {
            val data = CustomDriverData(it.path, it.metadata.name)
            DriverSettingChoice(
                DriverSettingMode.CUSTOM,
                data,
            )
        }

        val titleDriverSetting = NativeGameTitles.getDriverSettingForTitle(game.titleId)
        val selectedDriverName =
            if (titleDriverSetting.mode == DriverSettingMode.CUSTOM && titleDriverSetting.customPath != null) {
                installedDrivers.firstOrNull { it.customDriverData?.customPath == titleDriverSetting.customPath }
                    ?.customDriverData?.name
            } else {
                null
            }

        if (titleDriverSetting.mode == DriverSettingMode.CUSTOM && selectedDriverName != null) {
            _selectedDriverSetting.value = DriverSettingChoice(
                mode = DriverSettingMode.CUSTOM,
                customDriverData = CustomDriverData(
                    customPath = titleDriverSetting.customPath!!,
                    name = selectedDriverName,
                )
            )
        } else {
            val mode = if (titleDriverSetting.mode == DriverSettingMode.CUSTOM)
                DriverSettingMode.GLOBAL
            else
                titleDriverSetting.mode
            _selectedDriverSetting.value = DriverSettingChoice(mode = mode)
        }

        return listOf(globalDriverSetting) + listOf(systemDriverSetting) + installedDrivers
    }

    private val _driverSettingChoices = MutableStateFlow<List<DriverSettingChoice>>(listOf())
    val driverSettingChoices = _driverSettingChoices.asStateFlow()

    init {
        viewModelScope.launch {
            _driverSettingChoices.value = getDriverSettingsChoices()
        }
    }

    companion object {
        val GAME_KEY = object : CreationExtras.Key<Game> {}
        val Factory: ViewModelProvider.Factory = viewModelFactory {
            initializer {
                GameProfileEditViewModel(
                    this[GAME_KEY] as Game
                )
            }
        }
    }
}