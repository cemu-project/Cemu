package info.cemu.cemu.settings.input.device

import android.os.VibrationEffect
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.unit.dp
import info.cemu.cemu.R
import info.cemu.cemu.common.android.context.getDeviceVibrator
import info.cemu.cemu.common.ui.components.ScreenContent
import info.cemu.cemu.common.ui.components.SingleSelection
import info.cemu.cemu.common.ui.components.Slider
import info.cemu.cemu.common.ui.localization.tr
import info.cemu.cemu.nativeinterface.NativeInput

private val ControllerIndexChoices = (0..<NativeInput.MAX_CONTROLLERS).toList()

@Composable
fun DeviceInputSettingsScreen(navigateBack: () -> Unit) {
    DisposableEffect(Unit) {
        onDispose {
            NativeInput.saveInputs()
        }
    }

    val context = LocalContext.current
    val vibrator = remember { context.getDeviceVibrator() }
    var deviceControllerIndex by remember { mutableIntStateOf(NativeInput.getDeviceControllerIndex()) }
    val deviceControllerType = remember(deviceControllerIndex) {
        NativeInput.getControllerType(deviceControllerIndex)
    }

    ScreenContent(
        appBarText = tr("Device settings"),
        navigateBack = navigateBack,
    ) {
        SingleSelection(
            label = tr("Device controller"),
            initialChoice = { NativeInput.getDeviceControllerIndex() },
            choices = ControllerIndexChoices,
            isChoiceEnabled = { !NativeInput.isControllerDisabled(it) },
            choiceToString = { tr("Controller {0}", it + 1) },
            onChoiceChanged = {
                deviceControllerIndex = it
                NativeInput.setDeviceControllerIndex(it)
            }
        )

        if (deviceControllerType != NativeInput.EmulatedControllerType.VPAD) {
            Row(
                modifier = Modifier.padding(8.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Icon(
                    painter = painterResource(R.drawable.ic_warning),
                    contentDescription = null,
                )

                Spacer(modifier = Modifier.width(8.dp))

                Text(
                    text = tr("To use motion input, the controller type must be set to Wii U GamePad"),
                    style = MaterialTheme.typography.bodyMedium,
                )
            }
        }

        Slider(
            label = tr("Rumble"),
            initialValue = { (NativeInput.getDeviceRumble() * 100f).toInt() },
            steps = 19,
            valueFrom = 0,
            valueTo = 100,
            labelFormatter = { "$it%" },
            onValueChange = {
                val rumble = it / 100f

                NativeInput.setDeviceRumble(rumble)

                val amplitude = (255 * rumble).toInt()
                vibrator.cancel()
                if (amplitude in 1..255) {
                    vibrator.vibrate(VibrationEffect.createOneShot(500L, amplitude))
                }
            },
        )
    }
}
