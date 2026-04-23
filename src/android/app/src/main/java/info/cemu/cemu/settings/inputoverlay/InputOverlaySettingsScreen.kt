package info.cemu.cemu.settings.inputoverlay

import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.lifecycle.viewmodel.compose.viewModel
import info.cemu.cemu.common.settings.OverlayInputConfig
import info.cemu.cemu.common.ui.components.Header
import info.cemu.cemu.common.ui.components.ScreenContent
import info.cemu.cemu.common.ui.components.SingleSelection
import info.cemu.cemu.common.ui.components.Slider
import info.cemu.cemu.common.ui.components.Toggle
import info.cemu.cemu.common.ui.localization.tr
import info.cemu.cemu.nativeinterface.NativeInput

private val ControllerIndexChoices = (0..<NativeInput.MAX_CONTROLLERS).toList()

@Composable
fun InputOverlaySettingsScreen(
    viewModel: InputOverlaySettingsViewModel = viewModel(),
    navigateBack: () -> Unit,
) {
    val overlaySettings by viewModel.overlaySettings.collectAsState()

    @Composable
    fun VisibleInputToggle(inputName: String, input: OverlayInputConfig) {
        Toggle(
            label = inputName,
            checked = overlaySettings.inputVisibilityMap[input] ?: true,
            onCheckedChanged = { viewModel.setInputVisibility(input, it) }
        )
    }

    ScreenContent(
        appBarText = tr("Input overlay settings"),
        navigateBack = navigateBack
    ) {
        Toggle(
            label = tr("Input overlay"),
            description = tr("Enable input overlay"),
            checked = overlaySettings.isOverlayEnabled,
            onCheckedChanged = { viewModel.setOverlayEnabled(it) }
        )

        Toggle(
            label = tr("Vibrate"),
            description = tr("Enable vibrate on touch"),
            checked = overlaySettings.isVibrateOnTouchEnabled,
            onCheckedChanged = { viewModel.setVibrateOnTouch(it) }
        )

        Slider(
            label = tr("Inputs opacity"),
            value = overlaySettings.alpha,
            valueFrom = 0,
            steps = 16,
            valueTo = 255,
            onValueChange = { viewModel.setAlpha(it) },
            labelFormatter = { it.toString() },
        )

        SingleSelection(
            label = tr("Overlay controller"),
            choice = overlaySettings.controllerIndex,
            choices = ControllerIndexChoices,
            choiceToString = { tr("Controller {0}", it + 1) },
            onChoiceChanged = { viewModel.setControllerIndex(it) },
        )

        Header(tr("Visible Inputs"))

        VisibleInputToggle("A", OverlayInputConfig.BUTTON_A)
        VisibleInputToggle("B", OverlayInputConfig.BUTTON_B)
        VisibleInputToggle("X", OverlayInputConfig.BUTTON_X)
        VisibleInputToggle("Y", OverlayInputConfig.BUTTON_Y)
        VisibleInputToggle("1", OverlayInputConfig.BUTTON_ONE)
        VisibleInputToggle("2", OverlayInputConfig.BUTTON_TWO)
        VisibleInputToggle("C", OverlayInputConfig.BUTTON_C)
        VisibleInputToggle("Z", OverlayInputConfig.BUTTON_Z)
        VisibleInputToggle(tr("Home"), OverlayInputConfig.BUTTON_HOME)
        VisibleInputToggle("L", OverlayInputConfig.BUTTON_L)
        VisibleInputToggle("R", OverlayInputConfig.BUTTON_R)
        VisibleInputToggle(tr("Left stick click"), OverlayInputConfig.BUTTON_L_STICK_CLICK)
        VisibleInputToggle(tr("Right stick click"), OverlayInputConfig.BUTTON_R_STICK_CLICK)
        VisibleInputToggle(tr("Plus"), OverlayInputConfig.BUTTON_PLUS)
        VisibleInputToggle(tr("Minus"), OverlayInputConfig.BUTTON_MINUS)
        VisibleInputToggle("ZL", OverlayInputConfig.BUTTON_ZL)
        VisibleInputToggle("ZR", OverlayInputConfig.BUTTON_ZR)
        VisibleInputToggle(tr("Blow mic"), OverlayInputConfig.BUTTON_BLOW_MIC)

        VisibleInputToggle(tr("Left joystick"), OverlayInputConfig.JOYSTICK_LEFT)
        VisibleInputToggle(tr("Right joystick"), OverlayInputConfig.JOYSTICK_RIGHT)

        VisibleInputToggle(tr("D-pad"), OverlayInputConfig.DPAD)
    }
}
