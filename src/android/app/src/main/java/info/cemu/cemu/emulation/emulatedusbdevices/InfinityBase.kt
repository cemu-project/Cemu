package info.cemu.cemu.emulation.emulatedusbdevices

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.material3.Button
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import info.cemu.cemu.common.ui.components.NumericUIntField
import info.cemu.cemu.common.ui.components.SearchableDropdown
import info.cemu.cemu.common.ui.components.Toggle
import info.cemu.cemu.common.ui.localization.tr
import info.cemu.cemu.nativeinterface.NativeEmulatedUSBDevices
import info.cemu.cemu.nativeinterface.NativeSettings

@Composable
fun InfinityBaseTab(
    infinityFigures: Array<NativeEmulatedUSBDevices.InfinityFigure>,
    onCreate: (Long) -> Unit,
    infinitySlots: Array<String?>,
    installedFigures: Array<NativeEmulatedUSBDevices.InstalledFigure>,
    onDeleteInstalledFigure: (NativeEmulatedUSBDevices.InstalledFigure) -> Unit,
    onLoadFigure: (NativeEmulatedUSBDevices.InstalledFigure, Int) -> Unit,
    onClear: (Int) -> Unit
) {
    var showCreateDialog by remember { mutableStateOf(false) }
    var showInstalledFigures by remember { mutableStateOf(false) }
    var selectForSlot by remember { mutableStateOf<Int?>(null) }

    @Composable
    fun InfinityRow(label: String, index: Int) {
        val slot = infinitySlots[index]
        Row(
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            Column {
                Text(text = label, fontWeight = FontWeight.Medium, fontSize = 18.sp)
                Text(text = slot ?: tr("None"), fontSize = 16.sp)
            }
            Spacer(modifier = Modifier.weight(1f))
            Button(onClick = { selectForSlot = index }) { Text(tr("Select")) }
            Button(enabled = slot != null, onClick = { onClear(index) }) { Text(tr("Clear")) }
        }
        HorizontalDivider()
    }

    EmulatedUSBDeviceTabContent {
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(onClick = { showCreateDialog = true }) { Text(tr("Create")) }
            Button(onClick = { showInstalledFigures = true }) { Text(tr("Manage list")) }
        }

        InfinityRow("Play Set/Power Disc", 0)
        InfinityRow("Power Disc Two", 1)
        InfinityRow("Power Disc Three", 2)
        InfinityRow("Player One", 3)
        InfinityRow("Player One Ability One", 4)
        InfinityRow("Player One Ability Two", 5)
        InfinityRow("Player Two", 6)
        InfinityRow("Player Two Ability One", 7)
        InfinityRow("Player Two Ability Two", 8)
    }

    if (showCreateDialog) {
        InfinityFigureCreateDialog(
            infinityFigures = infinityFigures,
            onDismiss = { showCreateDialog = false },
            onCreate = { onCreate(it.toLong()) },
        )
    }

    if (showInstalledFigures) {
        InstalledFiguresDialog(
            label = tr("Infinity figures"),
            installedFigures = installedFigures,
            onDelete = onDeleteInstalledFigure,
            onDismiss = { showInstalledFigures = false })
    }

    val slot = selectForSlot
    if (slot != null) {
        SelectInstalledFigureDialog(
            installedFigures = installedFigures,
            onFigureSelected = { onLoadFigure(it, slot) },
            onDismiss = { selectForSlot = null })
    }
}

@Composable
private fun InfinityFigureCreateDialog(
    infinityFigures: Array<NativeEmulatedUSBDevices.InfinityFigure>,
    onDismiss: () -> Unit,
    onCreate: (UInt) -> Unit
) {
    var number by remember { mutableStateOf(0u) }

    FigureCreateDialog(
        title = tr("Infinity figure creator"),
        onDismiss = onDismiss,
        onCreate = { onCreate(number) },
    ) {
        SearchableDropdown(
            label = tr("Infinity figure"),
            choices = infinityFigures,
            choiceToString = { it.name },
            onChoiceSelected = { number = it.number })

        NumericUIntField(
            label = tr("Figure number"),
            value = number,
            onValueChanged = { number = it },
        )
    }
}
