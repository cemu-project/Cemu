package info.cemu.cemu.settings.input.controller.emulatedcontroller

import androidx.compose.runtime.Composable
import info.cemu.cemu.common.ui.localization.tr
import info.cemu.cemu.nativeinterface.NativeInput.ClassicButton

@Composable
fun ClassicControllerInputs(
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
            inputIdToString = ::classicControllerButtonToString,
            onInputClick = onInputClick,
            onInputLongClick = onInputLongClick,
            controlsMapping = controlsMapping,
        )
    }
    InputItemsGroup(
        groupName = tr("Buttons"),
        inputIds = listOf(
            ClassicButton.A,
            ClassicButton.B,
            ClassicButton.X,
            ClassicButton.Y,
            ClassicButton.L,
            ClassicButton.R,
            ClassicButton.ZL,
            ClassicButton.ZR,
            ClassicButton.PLUS,
            ClassicButton.MINUS,
            ClassicButton.HOME
        )
    )
    InputItemsGroup(
        groupName = tr("D-pad"),
        inputIds = listOf(
            ClassicButton.UP,
            ClassicButton.DOWN,
            ClassicButton.LEFT,
            ClassicButton.RIGHT
        )
    )
    InputItemsGroup(
        groupName = tr("Left Axis"),
        inputIds = listOf(
            ClassicButton.STICKL_UP,
            ClassicButton.STICKL_DOWN,
            ClassicButton.STICKL_LEFT,
            ClassicButton.STICKL_RIGHT
        )
    )
    InputItemsGroup(
        groupName = tr("Right Axis"),
        inputIds = listOf(
            ClassicButton.STICKR_UP,
            ClassicButton.STICKR_DOWN,
            ClassicButton.STICKR_LEFT,
            ClassicButton.STICKR_RIGHT
        )
    )
}

private fun classicControllerButtonToString(buttonId: Int) = when (buttonId) {
    ClassicButton.A -> "A"
    ClassicButton.B -> "B"
    ClassicButton.X -> "X"
    ClassicButton.Y -> "Y"
    ClassicButton.L -> "L"
    ClassicButton.R -> "R"
    ClassicButton.ZL -> "ZL"
    ClassicButton.ZR -> "ZR"
    ClassicButton.PLUS -> "+"
    ClassicButton.MINUS -> "-"
    ClassicButton.HOME -> tr("home")
    ClassicButton.UP -> tr("up")
    ClassicButton.DOWN -> tr("down")
    ClassicButton.LEFT -> tr("left")
    ClassicButton.RIGHT -> tr("right")
    ClassicButton.STICKL_UP -> tr("up")
    ClassicButton.STICKL_DOWN -> tr("down")
    ClassicButton.STICKL_LEFT -> tr("left")
    ClassicButton.STICKL_RIGHT -> tr("right")
    ClassicButton.STICKR_UP -> tr("up")
    ClassicButton.STICKR_DOWN -> tr("down")
    ClassicButton.STICKR_LEFT -> tr("left")
    ClassicButton.STICKR_RIGHT -> tr("right")
    else -> throw IllegalArgumentException("Invalid buttonId $buttonId for Classic controller type")
}