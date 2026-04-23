package info.cemu.cemu.emulation.emulatedusbdevices

import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.painter.Painter
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import info.cemu.cemu.R
import info.cemu.cemu.common.ui.components.GenericDialog
import info.cemu.cemu.common.ui.components.NumericUIntField
import info.cemu.cemu.common.ui.components.SearchableDropdown
import info.cemu.cemu.common.ui.components.Toggle
import info.cemu.cemu.common.ui.localization.tr
import info.cemu.cemu.nativeinterface.NativeEmulatedUSBDevices
import info.cemu.cemu.nativeinterface.NativeSettings

@Composable
fun DimensionsToypadTab(
    dimensionsMiniFigures: Array<NativeEmulatedUSBDevices.DimensionsMiniFigure>,
    installedFigures: Array<NativeEmulatedUSBDevices.InstalledFigure>,
    dimensionSlots: Array<String?>,
    onDeleteInstalledFigure: (NativeEmulatedUSBDevices.InstalledFigure) -> Unit,
    onLoadFigure: (NativeEmulatedUSBDevices.InstalledFigure, Int, Int) -> Unit,
    onCreate: (Long) -> Unit,
    onClear: (Int, Int) -> Unit,
    onTempRemove: (Int) -> Unit,
    onCancelRemove: (Int) -> Unit,
    onMoveFigure: (pad: Int, index: Int, oldPad: Int, oldIndex: Int) -> Unit,
) {
    var showCreateDialog by remember { mutableStateOf(false) }
    var showInstalledFigures by remember { mutableStateOf(false) }
    var selectForSlot by remember { mutableStateOf<Pair<Int, Int>?>(null) }
    var moveForSlot by remember { mutableStateOf<Pair<Int, Int>?>(null) }

    @Composable
    fun DimensionsRow(painter: Painter, pad: Int, index: Int) {
        val slot = dimensionSlots[index]
        Row(verticalAlignment = Alignment.CenterVertically) {
            Icon(painter = painter, modifier = Modifier.size(70.dp), contentDescription = null)
            Text(
                text = slot ?: tr("None"),
                fontSize = 22.sp,
                fontWeight = FontWeight.Medium,
            )
        }

        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(
                enabled = slot != null,
                onClick = {
                    onTempRemove(index)
                    moveForSlot = Pair(pad, index)
                },
            ) { Text(tr("Move")) }
            Button(onClick = { selectForSlot = Pair(pad, index) }) { Text(tr("Select")) }
            Button(enabled = slot != null, onClick = { onClear(pad, index) }) { Text(tr("Clear")) }
        }

        HorizontalDivider()
    }

    EmulatedUSBDeviceTabContent {
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(onClick = { showCreateDialog = true }) { Text(tr("Create")) }
            Button(onClick = { showInstalledFigures = true }) { Text(tr("Manage list")) }
        }

        DimensionsRow(painterResource(R.drawable.toypad_top_left), 2, 0)
        DimensionsRow(painterResource(R.drawable.toypad_center), 1, 1)
        DimensionsRow(painterResource(R.drawable.toypad_top_right), 3, 2)
        DimensionsRow(painterResource(R.drawable.toypad_bottom_left), 2, 3)
        DimensionsRow(painterResource(R.drawable.toypad_center_left), 2, 4)
        DimensionsRow(painterResource(R.drawable.toypad_center_right), 3, 5)
        DimensionsRow(painterResource(R.drawable.toypad_bottom_right), 3, 6)
    }

    if (showCreateDialog) {
        DimensionMiniFigureCreateDialog(
            dimensionsMiniFigures = dimensionsMiniFigures,
            onDismiss = { showCreateDialog = false },
            onCreate = { onCreate(it.toLong()) })
    }
    if (showInstalledFigures) {
        InstalledFiguresDialog(
            label = tr("Dimensions figures"),
            installedFigures = installedFigures,
            onDelete = onDeleteInstalledFigure,
            onDismiss = { showInstalledFigures = false })
    }

    val slot = selectForSlot
    if (slot != null) {
        SelectInstalledFigureDialog(
            installedFigures = installedFigures,
            onFigureSelected = {
                val (pad, index) = slot
                onLoadFigure(it, pad, index)
            },
            onDismiss = { selectForSlot = null },
        )
    }

    val moveSlot = moveForSlot
    if (moveSlot != null) {
        val (pad, index) = moveSlot
        MoveDimensionFigureDialog(
            dimensionSlots = dimensionSlots,
            currentIndex = index,
            onDismiss = {
                onCancelRemove(index)
                moveForSlot = null
            },
            onMoveSelected = { newPad, newIndex ->
                onMoveFigure(newPad, newIndex, pad, index)
                moveForSlot = null
            })
    }
}

@Composable
private fun DimensionMiniFigureCreateDialog(
    dimensionsMiniFigures: Array<NativeEmulatedUSBDevices.DimensionsMiniFigure>,
    onDismiss: () -> Unit,
    onCreate: (UInt) -> Unit
) {
    var number by remember { mutableStateOf(0u) }

    FigureCreateDialog(
        title = tr("Dimensions figure creator"),
        onDismiss = onDismiss,
        onCreate = { onCreate(number) },
    ) {
        SearchableDropdown(
            label = tr("Dimensions figure"),
            choices = dimensionsMiniFigures,
            choiceToString = { it.name },
            onChoiceSelected = { number = it.number })

        NumericUIntField(
            label = tr("Figure number"),
            value = number,
            onValueChanged = { number = it },
        )
    }
}

@Composable
fun MoveDimensionFigureDialog(
    dimensionSlots: Array<String?>,
    currentIndex: Int,
    onDismiss: () -> Unit,
    onMoveSelected: (pad: Int, index: Int) -> Unit,
) {
    GenericDialog(
        title = tr("Dimensions Figure Mover"),
        onDismissRequest = onDismiss,
    ) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween
        ) {
            MinifigSlot(2, 0, dimensionSlots[0], currentIndex, onMoveSelected)
            MinifigSlot(1, 1, dimensionSlots[1], currentIndex, onMoveSelected)
            MinifigSlot(3, 2, dimensionSlots[2], currentIndex, onMoveSelected)
        }

        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween
        ) {
            MinifigSlot(2, 3, dimensionSlots[3], currentIndex, onMoveSelected)
            MinifigSlot(2, 4, dimensionSlots[4], currentIndex, onMoveSelected)
            MinifigSlot(3, 5, dimensionSlots[5], currentIndex, onMoveSelected)
            MinifigSlot(3, 6, dimensionSlots[6], currentIndex, onMoveSelected)
        }
    }
}

@Composable
private fun MinifigSlot(
    pad: Int,
    index: Int,
    name: String?,
    currentIndex: Int,
    onMoveSelected: (pad: Int, index: Int) -> Unit
) {
    val shape = if (index == 1) CircleShape else RoundedCornerShape(6.dp)
    val isCurrent = index == currentIndex

    val borderColor = when {
        isCurrent -> MaterialTheme.colorScheme.primary
        else -> MaterialTheme.colorScheme.outline
    }

    Column(
        modifier = Modifier
            .padding(4.dp)
            .size(60.dp)
            .border(2.dp, borderColor, shape)
            .clip(shape)
            .padding(6.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.SpaceBetween
    ) {
        Text(
            name ?: tr("None"),
            fontSize = 10.sp,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis
        )
        IconButton(onClick = { onMoveSelected(pad, index) }) {
            Icon(
                painter = if (isCurrent) painterResource(R.drawable.ic_replace)
                else painterResource(R.drawable.ic_place_item),
                contentDescription = if (isCurrent) tr("Pick up and place")
                else tr("Move here")
            )
        }
    }
}
