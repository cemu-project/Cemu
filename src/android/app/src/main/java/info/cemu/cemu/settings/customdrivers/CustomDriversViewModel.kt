@file:OptIn(ExperimentalPathApi::class, ExperimentalUuidApi::class)

package info.cemu.cemu.settings.customdrivers

import android.content.Context
import android.net.Uri
import android.os.Build
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import info.cemu.cemu.common.customdrivers.DriverMetadata
import info.cemu.cemu.common.customdrivers.META_FILE_NAME
import info.cemu.cemu.common.customdrivers.SUPPORTED_SCHEMA_VERSION
import info.cemu.cemu.common.customdrivers.getCustomDriversDir
import info.cemu.cemu.common.customdrivers.parseInstalledDrivers
import info.cemu.cemu.common.io.decodeJsonFromFile
import info.cemu.cemu.common.io.unzip
import info.cemu.cemu.nativeinterface.NativeActiveSettings
import info.cemu.cemu.nativeinterface.NativeSettings
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch
import kotlin.io.path.ExperimentalPathApi
import kotlin.io.path.Path
import kotlin.io.path.createDirectories
import kotlin.io.path.deleteRecursively
import kotlin.io.path.exists
import kotlin.io.path.moveTo
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

data class Driver(
    val path: String,
    val metadata: DriverMetadata,
    val selected: Boolean = false,
)

enum class DriverInstallStatus {
    Installed,
    AlreadyInstalled,
    ErrorInstalling,
}

class CustomDriversViewModel : ViewModel() {
    private val selectedDriverPath = MutableStateFlow(NativeSettings.getCustomDriverPath())
    val isSystemDriverSelected = selectedDriverPath.map { it == null }.stateIn(
        viewModelScope,
        SharingStarted.WhileSubscribed(5000),
        false
    )

    private val _installedDrivers = MutableStateFlow<List<Driver>>(emptyList())
    val installedDrivers = _installedDrivers.asStateFlow()

    init {
        viewModelScope.launch {
            val selectedDriver = selectedDriverPath.value

            _installedDrivers.value = parseInstalledDrivers().map {
                Driver(
                    it.path,
                    it.metadata,
                    selected = selectedDriver == it.path,
                )
            }
        }
    }

    private val _isDriverInstallInProgress = MutableStateFlow(false)
    val isDriverInstallInProgress = _isDriverInstallInProgress.asStateFlow()

    fun installDriver(
        context: Context,
        driverZipUri: Uri,
        onInstallFinished: (DriverInstallStatus) -> Unit,
    ) {
        _isDriverInstallInProgress.value = true
        viewModelScope.launch(Dispatchers.IO) {
            val tempDir =
                Path(NativeActiveSettings.getUserDataPath()).resolve(Uuid.random().toString())

            try {
                tempDir.createDirectories()

                context.contentResolver.openInputStream(driverZipUri)?.use {
                    unzip(it, tempDir)
                }

                val metadata =
                    decodeJsonFromFile<DriverMetadata>(tempDir.resolve(META_FILE_NAME).toFile())
                if (metadata == null
                    || metadata.minApi > Build.VERSION.SDK_INT
                    || metadata.schemaVersion != SUPPORTED_SCHEMA_VERSION
                    || !tempDir.resolve(metadata.libraryName).exists()
                ) {
                    tempDir.deleteRecursively()
                    onInstallFinished(DriverInstallStatus.ErrorInstalling)
                    return@launch
                }

                if (_installedDrivers.value.any { it.metadata == metadata }) {
                    tempDir.deleteRecursively()
                    onInstallFinished(DriverInstallStatus.AlreadyInstalled)
                    return@launch
                }

                val customDriversDir = getCustomDriversDir()
                customDriversDir.createDirectories()
                val driverPath = tempDir.moveTo(customDriversDir.resolve(tempDir.fileName))

                _installedDrivers.value = _installedDrivers.value.toMutableList().apply {
                    val driver = Driver(
                        metadata = metadata,
                        path = driverPath.toString(),
                    )
                    add(driver)
                    sortBy { it.metadata.name }
                }

                onInstallFinished(DriverInstallStatus.Installed)
            } catch (exception: Exception) {
                tempDir.deleteRecursively()
                onInstallFinished(DriverInstallStatus.ErrorInstalling)
            } finally {
                _isDriverInstallInProgress.value = false
            }
        }
    }

    fun deleteDriver(driver: Driver) {
        if (!_installedDrivers.value.any { it == driver })
            return

        _installedDrivers.value -= driver
        if (selectedDriverPath.value == driver.path) {
            selectedDriverPath.value = null
            NativeSettings.setCustomDriverPath(null)
        }

        viewModelScope.launch(Dispatchers.IO) {
            Path(driver.path).toFile().deleteRecursively()
        }
    }

    fun setSystemDriverSelected() {
        if (selectedDriverPath.value == null)
            return

        val installedDrivers = _installedDrivers.value.toMutableList()
        val oldSelectedDriverIndex = installedDrivers.indexOfFirst { it.selected }
        if (oldSelectedDriverIndex != -1) {
            installedDrivers[oldSelectedDriverIndex] =
                installedDrivers[oldSelectedDriverIndex].copy(selected = false)
            _installedDrivers.value = installedDrivers
        }

        selectedDriverPath.value = null
        NativeSettings.setCustomDriverPath(null)
    }

    fun setDriverSelected(driver: Driver) {
        if (selectedDriverPath.value == driver.path)
            return

        val installedDrivers = _installedDrivers.value.toMutableList()

        val oldSelectedDriverIndex = installedDrivers.indexOfFirst { it.selected }
        if (oldSelectedDriverIndex != -1)
            installedDrivers[oldSelectedDriverIndex] =
                installedDrivers[oldSelectedDriverIndex].copy(selected = false)

        val newSelectedDriverIndex = installedDrivers.indexOf(driver)
        if (newSelectedDriverIndex == -1)
            return
        installedDrivers[newSelectedDriverIndex] = driver.copy(selected = true)

        _installedDrivers.value = installedDrivers

        NativeSettings.setCustomDriverPath(driver.path)
        selectedDriverPath.value = driver.path
    }
}