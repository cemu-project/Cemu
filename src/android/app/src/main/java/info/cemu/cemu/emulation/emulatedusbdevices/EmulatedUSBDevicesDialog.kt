package info.cemu.cemu.emulation.emulatedusbdevices

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.PrimaryTabRow
import androidx.compose.material3.Surface
import androidx.compose.material3.Tab
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import androidx.lifecycle.viewmodel.compose.viewModel
import info.cemu.cemu.R
import info.cemu.cemu.common.ui.localization.tr
import info.cemu.cemu.nativeinterface.NativeSettings

enum class EmulatedUSBDestination {
    SKYLANDERS_PORTAL, INFINITY_BASE, DIMENSIONS_TOYPAD,
}

@Composable
fun EmulatedUSBDevicesDialog(
    viewModel: EmulatedUSBDevicesViewModel = viewModel(),
    onDismiss: () -> Unit,
) {
    val enabledDestinations = remember {
        listOfNotNull(
            EmulatedUSBDestination.SKYLANDERS_PORTAL.takeIf { NativeSettings.isEmulateSkylanderPortalEnabled() },
            EmulatedUSBDestination.INFINITY_BASE.takeIf { NativeSettings.isEmulateInfinityBaseEnabled() },
            EmulatedUSBDestination.DIMENSIONS_TOYPAD.takeIf { NativeSettings.isEmulateDimensionsToypadEnabled() },
        )
    }
    var selectedDestination by rememberSaveable { mutableStateOf(enabledDestinations.firstOrNull()) }
    var lastFailedEvent by remember { mutableStateOf<UsbDeviceEvent?>(null) }

    val dimensionSlots by viewModel.dimensionsSlots.state.collectAsState()
    val skylanderSlots by viewModel.skylanderSlots.state.collectAsState()
    val infinitySlots by viewModel.infinitySlots.state.collectAsState()
    val installedSkylanderFigures by viewModel.installedSkylanderFigures.state.collectAsState()
    val installedDimensionMiniFigures by viewModel.installedDimensionMiniFigures.state.collectAsState()
    val installedInfinityFigures by viewModel.installedInfinityFigures.state.collectAsState()

    LaunchedEffect(Unit) {
        viewModel.events.collect { event ->
            lastFailedEvent = event
        }
    }

    Dialog(
        onDismissRequest = onDismiss,
        properties = DialogProperties(usePlatformDefaultWidth = false),
    ) {
        Surface(
            modifier = Modifier.fillMaxSize(),
            color = MaterialTheme.colorScheme.surface,
        ) {
            Column {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    IconButton(onClick = onDismiss) {
                        Icon(
                            painter = painterResource(R.drawable.ic_close),
                            contentDescription = null,
                        )
                    }
                    Text(
                        text = tr("Emulated USB Devices"),
                        fontSize = 18.sp,
                    )
                }

                if (enabledDestinations.isEmpty()) {
                    Box(
                        modifier = Modifier.fillMaxSize(),
                        contentAlignment = Alignment.Center
                    ) {
                        Text(tr("No emulated USB device is enabled"))
                    }
                    return@Column
                }

                PrimaryTabRow(selectedTabIndex = enabledDestinations.indexOf(selectedDestination)) {
                    enabledDestinations.forEach { destination ->
                        Tab(
                            selected = destination == selectedDestination,
                            onClick = { selectedDestination = destination },
                            text = {
                                Text(
                                    text = when (destination) {
                                        EmulatedUSBDestination.SKYLANDERS_PORTAL -> tr("Skylanders Portal")
                                        EmulatedUSBDestination.INFINITY_BASE -> tr("Infinity Base")
                                        EmulatedUSBDestination.DIMENSIONS_TOYPAD -> tr("Dimensions Toypad")
                                    },
                                    maxLines = 2,
                                    overflow = TextOverflow.Ellipsis,
                                )
                            })
                    }
                }

                when (selectedDestination) {
                    EmulatedUSBDestination.SKYLANDERS_PORTAL -> SkylandersPortalTab(
                        skylanderFigures = viewModel.skylanderFigures,
                        onCreate = viewModel::createSkylanderFigure,
                        skylanderSlots = skylanderSlots,
                        installedFigures = installedSkylanderFigures,
                        onDeleteInstalledFigure = viewModel::deleteInstalledSkylanderFigure,
                        onLoadFigure = viewModel::loadSkylanderFigure,
                        onClear = viewModel::clearSkylandersFigure,
                    )

                    EmulatedUSBDestination.INFINITY_BASE -> InfinityBaseTab(
                        infinityFigures = viewModel.infinityFigures,
                        onCreate = viewModel::createInfinityFigure,
                        infinitySlots = infinitySlots,
                        installedFigures = installedInfinityFigures,
                        onDeleteInstalledFigure = viewModel::deleteInstalledInfinityFigure,
                        onLoadFigure = viewModel::loadInfinityFigure,
                        onClear = viewModel::clearInfinityFigure,
                    )

                    EmulatedUSBDestination.DIMENSIONS_TOYPAD -> DimensionsToypadTab(
                        dimensionsMiniFigures = viewModel.dimensionsMiniFigures,
                        onCreate = viewModel::createDimensionsMiniFigure,
                        dimensionSlots = dimensionSlots,
                        onClear = viewModel::clearDimensionsFigure,
                        installedFigures = installedDimensionMiniFigures,
                        onDeleteInstalledFigure = viewModel::deleteInstalledDimensionsFigure,
                        onLoadFigure = viewModel::loadDimensionsFigure,
                        onTempRemove = viewModel::tempRemoveDimensionsFigure,
                        onCancelRemove = viewModel::cancelRemoveDimensionsFigure,
                        onMoveFigure = viewModel::moveDimensionsFigure,
                    )

                    else -> {}
                }
            }
        }
    }

    if (lastFailedEvent != null) {
        AlertDialog(
            onDismissRequest = { lastFailedEvent = null },
            confirmButton = {
                TextButton(onClick = { lastFailedEvent = null }) { Text(tr("Close")) }
            },
            title = { Text(tr("Operation failed")) },
            text = {
                Text(
                    when (lastFailedEvent) {
                        UsbDeviceEvent.CreateFailed -> tr("Failed to create figure")
                        UsbDeviceEvent.LoadFailed -> tr("Failed to load figure")
                        else -> ""
                    }
                )
            })
    }
}
