package info.cemu.cemu.settings.input

import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.lifecycle.compose.dropUnlessResumed
import info.cemu.cemu.common.ui.components.Button
import info.cemu.cemu.common.ui.components.ScreenContent
import info.cemu.cemu.common.ui.localization.controllerTypeToString
import info.cemu.cemu.common.ui.localization.tr
import info.cemu.cemu.nativeinterface.NativeInput

@Composable
fun InputSettingsScreen(
    navigateBack: () -> Unit,
    goToInputOverlaySettings: () -> Unit,
    goToControllerSettings: (controllerIndex: Int) -> Unit,
    goToHostInputSettings: () -> Unit,
    goToHotkeySettings: () -> Unit,
) {
    val controllers = remember {
        (0..<NativeInput.MAX_CONTROLLERS).map { it to NativeInput.getControllerType(it) }
    }
    ScreenContent(
        appBarText = tr("Input settings"),
        navigateBack = navigateBack,
    ) {
        Button(
            label = tr("Input overlay settings"),
            onClick = dropUnlessResumed { goToInputOverlaySettings() },
        )
        Button(
            label = tr("Device settings"),
            onClick = dropUnlessResumed { goToHostInputSettings() },
        )
        Button(
            label = tr("Hotkey settings"),
            onClick = dropUnlessResumed { goToHotkeySettings() },
        )
        controllers.forEach { (controllerIndex, controllerEmulatedType) ->
            Button(
                label = tr("Controller {0}", controllerIndex + 1),
                description = tr(
                    "Emulated controller: {0}",
                    controllerTypeToString(controllerEmulatedType),
                ),
                onClick = dropUnlessResumed { goToControllerSettings(controllerIndex) },
            )
        }
    }
}
