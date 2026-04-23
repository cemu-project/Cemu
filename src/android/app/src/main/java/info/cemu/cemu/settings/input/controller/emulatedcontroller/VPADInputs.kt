package info.cemu.cemu.settings.input.controller.emulatedcontroller

import androidx.compose.runtime.Composable
import info.cemu.cemu.common.ui.components.Toggle
import info.cemu.cemu.common.ui.localization.tr
import info.cemu.cemu.nativeinterface.NativeInput
import info.cemu.cemu.nativeinterface.NativeInput.VPADButton

@Composable
fun VPADInputs(
    controllerIndex: Int,
    onInputClick: (String, Int) -> Unit,
    onInputLongClick: (Int) -> Unit,
    controlsMapping: Map<Int, String>,
) {
    @Composable
    fun InputItemsGroup(
        groupName: String,
        inputIds: List<Int>,
    ) {
        InputItemsGroup(
            groupName = groupName,
            inputIds = inputIds,
            inputIdToString = { vpadButtonToString(it) },
            onInputClick = onInputClick,
            onInputLongClick = onInputLongClick,
            controlsMapping = controlsMapping,
        )
    }
    InputItemsGroup(
        groupName = tr("Buttons"),
        inputIds = listOf(
            VPADButton.A,
            VPADButton.B,
            VPADButton.X,
            VPADButton.Y,
            VPADButton.L,
            VPADButton.R,
            VPADButton.ZL,
            VPADButton.ZR,
            VPADButton.PLUS,
            VPADButton.MINUS
        )
    )
    InputItemsGroup(
        groupName = tr("D-pad"),
        inputIds = listOf(
            VPADButton.UP,
            VPADButton.DOWN,
            VPADButton.LEFT,
            VPADButton.RIGHT
        )
    )
    InputItemsGroup(
        groupName = tr("Left Axis"),
        inputIds = listOf(
            VPADButton.STICKL,
            VPADButton.STICKL_UP,
            VPADButton.STICKL_DOWN,
            VPADButton.STICKL_LEFT,
            VPADButton.STICKL_RIGHT
        )
    )
    InputItemsGroup(
        groupName = tr("Right Axis"),
        inputIds = listOf(
            VPADButton.STICKR,
            VPADButton.STICKR_UP,
            VPADButton.STICKR_DOWN,
            VPADButton.STICKR_LEFT,
            VPADButton.STICKR_RIGHT
        )
    )
    InputItemsGroup(
        groupName = tr("Extra"),
        inputIds = listOf(
            VPADButton.MIC,
            VPADButton.HOME,
            VPADButton.SCREEN
        )
    )
    Toggle(
        label = tr("Toggle screen"),
        description = tr("Makes the \"show screen\" button toggle between the TV and gamepad screens"),
        initialCheckedState = { NativeInput.getVPADScreenToggle(controllerIndex) },
        onCheckedChanged = { NativeInput.setVPADScreenToggle(controllerIndex, it) }
    )
}

private fun vpadButtonToString(buttonId: Int) = when (buttonId) {
    VPADButton.A -> "A"
    VPADButton.B -> "B"
    VPADButton.X -> "X"
    VPADButton.Y -> "Y"
    VPADButton.L -> "L"
    VPADButton.R -> "R"
    VPADButton.ZL -> "ZL"
    VPADButton.ZR -> "ZR"
    VPADButton.PLUS -> "+"
    VPADButton.MINUS -> "-"
    VPADButton.UP -> tr("up")
    VPADButton.DOWN -> tr("down")
    VPADButton.LEFT -> tr("left")
    VPADButton.RIGHT -> tr("right")
    VPADButton.STICKL -> tr("click")
    VPADButton.STICKR -> tr("click")
    VPADButton.STICKL_UP -> tr("up")
    VPADButton.STICKL_DOWN -> tr("down")
    VPADButton.STICKL_LEFT -> tr("left")
    VPADButton.STICKL_RIGHT -> tr("right")
    VPADButton.STICKR_UP -> tr("up")
    VPADButton.STICKR_DOWN -> tr("down")
    VPADButton.STICKR_LEFT -> tr("left")
    VPADButton.STICKR_RIGHT -> tr("right")
    VPADButton.MIC -> tr("blow mic")
    VPADButton.SCREEN -> tr("show screen")
    VPADButton.HOME -> tr("home")
    else -> throw IllegalArgumentException("Invalid buttonId $buttonId for VPAD controller type")
}
