package info.cemu.cemu.settings.input.hotkeys

import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.focusable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.sizeIn
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.mutableStateSetOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.input.key.KeyEventType
import androidx.compose.ui.input.key.key
import androidx.compose.ui.input.key.nativeKeyCode
import androidx.compose.ui.input.key.onKeyEvent
import androidx.compose.ui.input.key.type
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Dialog
import androidx.lifecycle.viewmodel.compose.viewModel
import info.cemu.cemu.common.android.inputevent.isFromPhysicalController
import info.cemu.cemu.common.android.keyevent.keyCodeToDisplayName
import info.cemu.cemu.common.settings.HotkeyAction
import info.cemu.cemu.common.settings.HotkeyCombo
import info.cemu.cemu.common.ui.components.ScreenContent
import info.cemu.cemu.common.ui.localization.tr
import kotlinx.coroutines.delay

@Composable
fun HotkeySettingsScreen(
    navigateBack: () -> Unit,
    hotkeyViewModel: HotkeyViewModel = viewModel(),
) {
    val hotkeys by hotkeyViewModel.hotkeys.collectAsState()
    var selectedAction by rememberSaveable { mutableStateOf<HotkeyAction?>(null) }

    ScreenContent(
        appBarText = tr("Hotkey settings"),
        navigateBack = navigateBack,
    ) {
        HotkeyAction.entries.forEach { action ->
            val combo = hotkeys[action]

            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .combinedClickable(
                        onClick = { selectedAction = action },
                        onLongClick = { hotkeyViewModel.clearHotkeyMapping(action) }
                    )
                    .padding(horizontal = 16.dp, vertical = 12.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Column(modifier = Modifier.weight(1f)) {
                    Text(
                        text = hotkeyActionToString(action),
                        style = MaterialTheme.typography.bodyLarge
                    )

                    if (combo != null) {
                        FlowRow(horizontalArrangement = Arrangement.spacedBy(2.dp)) {
                            combo.keys.forEach { key ->
                                KeyChip(key)
                            }
                        }
                    } else {
                        Text(
                            text = tr("Not set"),
                            style = MaterialTheme.typography.bodySmall,
                        )
                    }
                }
            }
        }
    }

    val currentAction = selectedAction
    if (currentAction != null) {
        HotkeyBindingDialog(
            onDismiss = { selectedAction = null },
            mapHotkeyCombo = { keys ->
                hotkeyViewModel.setHotkeyMapping(currentAction, HotkeyCombo(keys))
                selectedAction = null
            },
            onClear = {
                hotkeyViewModel.clearHotkeyMapping(currentAction)
                selectedAction = null
            },
        )
    }
}

@Composable
private fun HotkeyBindingDialog(
    onDismiss: () -> Unit,
    onClear: () -> Unit,
    mapHotkeyCombo: (Set<Int>) -> Unit,
) {
    val pressedKeys = rememberSaveable {
        mutableStateSetOf<Int>()
    }

    val focusRequester = remember { FocusRequester() }

    LaunchedEffect(Unit) {
        focusRequester.requestFocus()
    }

    if (pressedKeys.isNotEmpty()) {
        LaunchedEffect(Unit) {
            delay(1500)

            if (pressedKeys.isEmpty()) {
                return@LaunchedEffect
            }

            mapHotkeyCombo(pressedKeys)
        }
    }

    Dialog(onDismissRequest = onDismiss) {
        Card(
            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainer),
            modifier = Modifier
                .sizeIn(maxWidth = 560.dp, maxHeight = 560.dp)
                .onKeyEvent {
                    if (!it.nativeKeyEvent.isFromPhysicalController()) {
                        return@onKeyEvent false
                    }

                    val keyCode = it.key.nativeKeyCode

                    when (it.type) {
                        KeyEventType.KeyDown -> pressedKeys.add(keyCode)
                        KeyEventType.KeyUp -> pressedKeys.remove(keyCode)
                    }

                    return@onKeyEvent true
                }
                .focusRequester(focusRequester)
                .focusable()
                .fillMaxWidth(),
            shape = RoundedCornerShape(16.dp),
        ) {
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(16.dp)
            ) {
                Text(
                    text = tr("Press a key combination"),
                    style = MaterialTheme.typography.titleMedium,
                    modifier = Modifier.padding(end = 16.dp),
                )

                if (pressedKeys.isEmpty()) {
                    Text(
                        text = tr("Press a button"),
                        modifier = Modifier.padding(end = 16.dp),
                    )
                } else {
                    Text(
                        text = tr("Hold to confirm"),
                        modifier = Modifier.padding(end = 16.dp),
                    )

                    FlowRow(horizontalArrangement = Arrangement.spacedBy(2.dp)) {
                        pressedKeys.forEach { key ->
                            KeyChip(key)
                        }
                    }
                }

                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(top = 8.dp),
                    horizontalArrangement = Arrangement.End
                ) {
                    TextButton(onClick = onClear) { Text(tr("Clear")) }
                    Spacer(modifier = Modifier.width(8.dp))
                    TextButton(onClick = onDismiss) { Text(tr("Cancel")) }
                }
            }
        }
    }
}

private fun hotkeyActionToString(hotkeyAction: HotkeyAction) = when (hotkeyAction) {
    HotkeyAction.QUIT -> tr("Quit")
    HotkeyAction.TOGGLE_MENU -> tr("Toggle menu")
    HotkeyAction.SHOW_EMULATED_USB_DEVICES_DIALOG -> tr("Show Emulated USB Devices dialog")
}

@Composable
private fun KeyChip(key: Int) {
    Surface(
        shape = RoundedCornerShape(8.dp),
        color = MaterialTheme.colorScheme.secondaryContainer,
        modifier = Modifier.padding(vertical = 4.dp),
        tonalElevation = 2.dp,
    ) {
        Text(
            text = keyCodeToDisplayName(key),
            modifier = Modifier.padding(horizontal = 8.dp, vertical = 2.dp),
            style = MaterialTheme.typography.bodyMedium,
        )
    }
}
