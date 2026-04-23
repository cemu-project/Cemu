package info.cemu.cemu.settings.input.controller.emulatedcontroller

import androidx.compose.runtime.Composable
import info.cemu.cemu.common.ui.localization.tr
import info.cemu.cemu.nativeinterface.NativeInput.ProButton

@Composable
fun ProControllerInputs(
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
            inputIdToString = { proControllerButtonToString(it) },
            onInputClick = onInputClick,
            onInputLongClick = onInputLongClick,
            controlsMapping = controlsMapping,
        )
    }
    InputItemsGroup(
        groupName = tr("Buttons"),
        inputIds = listOf(
            ProButton.A,
            ProButton.B,
            ProButton.X,
            ProButton.Y,
            ProButton.L,
            ProButton.R,
            ProButton.ZL,
            ProButton.ZR,
            ProButton.PLUS,
            ProButton.MINUS,
            ProButton.HOME
        )
    )
    InputItemsGroup(
        groupName = tr("D-pad"),
        inputIds = listOf(
            ProButton.UP,
            ProButton.DOWN,
            ProButton.LEFT,
            ProButton.RIGHT
        )
    )
    InputItemsGroup(
        groupName = tr("Left Axis"),
        inputIds = listOf(
            ProButton.STICKL,
            ProButton.STICKL_UP,
            ProButton.STICKL_DOWN,
            ProButton.STICKL_LEFT,
            ProButton.STICKL_RIGHT
        )
    )
    InputItemsGroup(
        groupName = tr("Right Axis"),
        inputIds = listOf(
            ProButton.STICKR,
            ProButton.STICKR_UP,
            ProButton.STICKR_DOWN,
            ProButton.STICKR_LEFT,
            ProButton.STICKR_RIGHT
        )
    )
}

private fun proControllerButtonToString(buttonId: Int) = when (buttonId) {
    ProButton.A -> "A"
    ProButton.B -> "B"
    ProButton.X -> "X"
    ProButton.Y -> "Y"
    ProButton.L -> "L"
    ProButton.R -> "R"
    ProButton.ZL -> "ZL"
    ProButton.ZR -> "ZR"
    ProButton.PLUS -> "+"
    ProButton.MINUS -> "-"
    ProButton.HOME -> tr("home")
    ProButton.UP -> tr("up")
    ProButton.DOWN -> tr("down")
    ProButton.LEFT -> tr("left")
    ProButton.RIGHT -> tr("right")
    ProButton.STICKL -> tr("click")
    ProButton.STICKR -> tr("click")
    ProButton.STICKL_UP -> tr("up")
    ProButton.STICKL_DOWN -> tr("down")
    ProButton.STICKL_LEFT -> tr("left")
    ProButton.STICKL_RIGHT -> tr("right")
    ProButton.STICKR_UP -> tr("up")
    ProButton.STICKR_DOWN -> tr("down")
    ProButton.STICKR_LEFT -> tr("left")
    ProButton.STICKR_RIGHT -> tr("right")
    else -> throw IllegalArgumentException("Invalid buttonId $buttonId for Pro controller type")
}