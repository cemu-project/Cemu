package info.cemu.cemu.settings.input.controller.emulatedcontroller

import androidx.compose.runtime.Composable
import info.cemu.cemu.common.ui.localization.tr
import info.cemu.cemu.nativeinterface.NativeInput.WiimoteButton

@Composable
fun WiimoteControllerInputs(
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
            inputIdToString = { wiimoteButtonItToString(it) },
            onInputClick = onInputClick,
            onInputLongClick = onInputLongClick,
            controlsMapping = controlsMapping,
        )
    }
    InputItemsGroup(
        groupName = tr("Buttons"),
        inputIds = listOf(
            WiimoteButton.A,
            WiimoteButton.B,
            WiimoteButton.ONE,
            WiimoteButton.TWO,
            WiimoteButton.NUNCHUCK_Z,
            WiimoteButton.NUNCHUCK_C,
            WiimoteButton.PLUS,
            WiimoteButton.MINUS,
            WiimoteButton.HOME
        )
    )
    InputItemsGroup(
        groupName = tr("Nunchuck"),
        inputIds = listOf(
            WiimoteButton.UP,
            WiimoteButton.DOWN,
            WiimoteButton.LEFT,
            WiimoteButton.RIGHT
        )
    )
    InputItemsGroup(
        groupName = tr("Right Axis"),
        inputIds = listOf(
            WiimoteButton.NUNCHUCK_UP,
            WiimoteButton.NUNCHUCK_DOWN,
            WiimoteButton.NUNCHUCK_LEFT,
            WiimoteButton.NUNCHUCK_RIGHT
        )
    )
}

private fun wiimoteButtonItToString(buttonId: Int) = when (buttonId) {
    WiimoteButton.A -> "A"
    WiimoteButton.B -> "B"
    WiimoteButton.ONE -> "1"
    WiimoteButton.TWO -> "2"
    WiimoteButton.NUNCHUCK_Z -> "Z"
    WiimoteButton.NUNCHUCK_C -> "C"
    WiimoteButton.PLUS -> "+"
    WiimoteButton.MINUS -> "-"
    WiimoteButton.UP -> tr("up")
    WiimoteButton.DOWN -> tr("down")
    WiimoteButton.LEFT -> tr("left")
    WiimoteButton.RIGHT -> tr("right")
    WiimoteButton.NUNCHUCK_UP -> tr("up")
    WiimoteButton.NUNCHUCK_DOWN -> tr("down")
    WiimoteButton.NUNCHUCK_LEFT -> tr("left")
    WiimoteButton.NUNCHUCK_RIGHT -> tr("right")
    WiimoteButton.HOME -> tr("home")
    else -> throw IllegalArgumentException("Invalid buttonId $buttonId for Wiimote controller type")
}
