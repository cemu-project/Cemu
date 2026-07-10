package info.cemu.cemu.settings.input.controller

import android.os.VibrationEffect
import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.sizeIn
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.util.fastRoundToInt
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import androidx.compose.ui.window.Popup
import androidx.lifecycle.viewmodel.MutableCreationExtras
import androidx.lifecycle.viewmodel.compose.viewModel
import info.cemu.cemu.R
import info.cemu.cemu.common.android.inputdevice.tryUseVibrator
import info.cemu.cemu.common.input.GamepadInputSource
import info.cemu.cemu.common.ui.components.Header
import info.cemu.cemu.common.ui.components.ScreenContent
import info.cemu.cemu.common.ui.components.SingleSelection
import info.cemu.cemu.common.ui.components.Slider
import info.cemu.cemu.common.ui.components.Toggle
import info.cemu.cemu.common.ui.extensions.showMessage
import info.cemu.cemu.common.ui.localization.controllerTypeToString
import info.cemu.cemu.common.ui.localization.tr
import info.cemu.cemu.nativeinterface.NativeInput
import info.cemu.cemu.nativeinterface.NativeInput.EmulatedControllerType
import info.cemu.cemu.settings.input.controller.emulatedcontroller.ClassicControllerInputs
import info.cemu.cemu.settings.input.controller.emulatedcontroller.ProControllerInputs
import info.cemu.cemu.settings.input.controller.emulatedcontroller.VPADInputs
import info.cemu.cemu.settings.input.controller.emulatedcontroller.WiimoteControllerInputs

@Composable
fun ControllerInputSettingsScreen(
    navigateBack: () -> Unit,
    controllerIndex: Int,
    viewModel: ControllersViewModel = viewModel(
        factory = ControllersViewModel.Factory, extras = MutableCreationExtras().apply {
            set(ControllersViewModel.CONTROLLER_INDEX_KEY, controllerIndex)
        }),
) {
    DisposableEffect(Unit) {
        onDispose {
            viewModel.save()
        }
    }

    val controllers by viewModel.controllers.collectAsState()
    val buttonToBind by viewModel.buttonToBind.collectAsState()
    val controllerType by viewModel.controllerType.collectAsState()
    val controls by viewModel.controls.collectAsState()
    val activeController by viewModel.activeController.collectAsState()

    var showMapAllInputsDialog by rememberSaveable { mutableStateOf(false) }
    var showControllerSettingsDialog by rememberSaveable { mutableStateOf(false) }

    val snackbarHostState = remember { SnackbarHostState() }
    val coroutineScope = rememberCoroutineScope()

    fun onInputClick(buttonName: String, buttonId: Int) {
        viewModel.setButtonToBind(ButtonInfo(buttonName, buttonId))
    }

    fun onInputLongClick(buttonId: Int) {
        viewModel.clearButtonMapping(buttonId)
    }

    fun refreshControllers(onControllersAvailable: () -> Unit) {
        if (viewModel.refreshAvailableControllers()) {
            onControllersAvailable()
        } else {
            snackbarHostState.showMessage(coroutineScope, tr("No controllers available"))
        }
    }

    ScreenContent(
        snackbarHost = { SnackbarHost(hostState = snackbarHostState) },
        appBarText = tr("Controller {0}", controllerIndex + 1),
        navigateBack = navigateBack,
    ) {
        SingleSelection(
            isChoiceEnabled = viewModel::isControllerTypeAllowed,
            label = tr("Emulated controller"),
            initialChoice = { controllerType },
            choices = listOf(
                EmulatedControllerType.DISABLED,
                EmulatedControllerType.VPAD,
                EmulatedControllerType.PRO,
                EmulatedControllerType.CLASSIC,
                EmulatedControllerType.WIIMOTE
            ),
            choiceToString = { controllerTypeToString(it) },
            onChoiceChanged = viewModel::setControllerType
        )

        if (controllerType == EmulatedControllerType.DISABLED) {
            return@ScreenContent
        }

        Row(
            modifier = Modifier.padding(8.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            Button(onClick = {
                refreshControllers { showMapAllInputsDialog = true }
            }) {
                Text(tr("Setup all inputs"))
            }

            Button(onClick = {
                refreshControllers { showControllerSettingsDialog = true }
            }) {
                Text(tr("Controller settings"))
            }
        }


        when (controllerType) {
            EmulatedControllerType.VPAD -> VPADInputs(
                controllerIndex = controllerIndex,
                onInputClick = ::onInputClick,
                onInputLongClick = ::onInputLongClick,
                controlsMapping = controls,
            )

            EmulatedControllerType.PRO -> ProControllerInputs(
                onInputClick = ::onInputClick,
                onInputLongClick = ::onInputLongClick,
                controlsMapping = controls,
            )

            EmulatedControllerType.CLASSIC -> ClassicControllerInputs(
                onInputClick = ::onInputClick,
                onInputLongClick = ::onInputLongClick,
                controlsMapping = controls,
            )

            EmulatedControllerType.WIIMOTE -> WiimoteControllerInputs(
                onInputClick = ::onInputClick,
                onInputLongClick = ::onInputLongClick,
                controlsMapping = controls,
            )
        }
    }

    buttonToBind?.let {
        InputBindingPopup(
            buttonName = it.name,
            mapKeyEvent = { event ->
                viewModel.mapKeyEvent(event, it.id)
                viewModel.clearButtonToBind()
            },
            mapMotionEvent = { event ->
                if (viewModel.tryMapMotionEvent(event, it.id)) {
                    viewModel.clearButtonToBind()
                }
            },
            onClear = {
                viewModel.clearButtonMapping(it.id)
                viewModel.clearButtonToBind()
            },
            onDismiss = {
                viewModel.clearButtonToBind()
            },
        )
    }

    if (showMapAllInputsDialog) {
        ControllerSelectDialog(
            controllers = controllers,
            onDismissRequest = { showMapAllInputsDialog = false },
            onSelect = { viewModel.mapAllInputs(it.id) },
        )
    }

    if (showControllerSettingsDialog) {
        ControllerSettingsDialog(
            controllers = controllers,
            controllerType = controllerType,
            activeController = activeController,
            onSetActiveController = viewModel::setActiveController,
            onSetControllerSettings = viewModel::setControllerSettings,
            onDismissRequest = { showControllerSettingsDialog = false })
    }
}

@Composable
private fun ControllerSettingsDialog(
    onDismissRequest: () -> Unit,
    controllerType: Int,
    activeController: ActiveController?,
    onSetActiveController: (InputController) -> Unit,
    onSetControllerSettings: (InputController, NativeInput.ControllerSettings) -> Unit,
    controllers: List<InputController>,
) {


    fun updateSettings(settings: NativeInput.ControllerSettings) {
        val (controller, _) = activeController ?: return
        onSetControllerSettings(controller, settings)
    }

    Dialog(
        onDismissRequest = onDismissRequest,
        properties = DialogProperties(usePlatformDefaultWidth = false)
    ) {
        Card(
            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface),
            modifier = Modifier.fillMaxSize(),
        ) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                IconButton(onClick = onDismissRequest) {
                    Icon(
                        painter = painterResource(R.drawable.ic_close),
                        contentDescription = null,
                    )
                }
                Text(
                    text = tr("Controller settings"),
                    fontSize = 18.sp,
                )
            }

            HorizontalDivider()

            Column(
                modifier = Modifier
                    .verticalScroll(rememberScrollState())
                    .padding(horizontal = 8.dp)
            ) {
                SingleSelection(
                    label = tr("Controller"),
                    choice = activeController?.controller,
                    choices = controllers,
                    choiceToString = { it?.name ?: "" },
                    onChoiceChanged = { onSetActiveController(it!!) },
                )

                val (controller, settings) = activeController ?: return@Column

                val canUseMotion =
                    controllerType == EmulatedControllerType.VPAD && controller.hasMotion

                Toggle(
                    label = tr("Use motion"),
                    checked = settings.motion && canUseMotion,
                    onCheckedChanged = { updateSettings(settings.copy(motion = it)) },
                    enabled = canUseMotion,
                    description = when {
                        controllerType != EmulatedControllerType.VPAD -> tr("Requires Wii U GamePad")
                        !controller.hasMotion -> tr("Controller has no motion sensors")
                        else -> null
                    },
                )

                Slider(
                    label = tr("Rumble"),
                    enabled = controller.hasRumble,
                    value = (settings.rumble * 100f).fastRoundToInt(),
                    valueFrom = 0,
                    steps = 19,
                    valueTo = 100,
                    labelFormatter = { "$it%" },
                    onValueChange = {
                        val rumble = it / 100f

                        updateSettings(settings.copy(rumble = rumble))

                        val amplitude = (255 * rumble).toInt()
                        InputDevice.getDevice(controller.id)?.tryUseVibrator {
                            cancel()
                            if (amplitude in 1..255) {
                                vibrate(VibrationEffect.createOneShot(500L, amplitude))
                            }
                        }
                    },
                )

                AxisGroup(
                    label = tr("Axis"),
                    values = settings.axis,
                    onValuesChanged = { updateSettings(settings.copy(axis = it)) },
                )

                AxisGroup(
                    label = tr("Rotation"),
                    values = settings.rotation,
                    onValuesChanged = { updateSettings(settings.copy(rotation = it)) },
                )

                AxisGroup(
                    label = tr("Trigger"),
                    values = settings.trigger,
                    onValuesChanged = { updateSettings(settings.copy(trigger = it)) },
                )
            }
        }
    }
}

@Composable
private fun AxisGroup(
    label: String,
    values: NativeInput.AxisSetting,
    onValuesChanged: (NativeInput.AxisSetting) -> Unit
) {
    Header(label)
    Slider(
        label = tr("Deadzone"),
        value = (values.deadzone * 100f).fastRoundToInt(),
        valueFrom = 0,
        valueTo = 100,
        labelFormatter = { "$it%" },
        onValueChange = { onValuesChanged(values.copy(deadzone = it / 100f)) },
    )
    Slider(
        label = tr("Range"),
        value = (values.range * 100f).fastRoundToInt(),
        valueFrom = 50,
        valueTo = 200,
        labelFormatter = { "$it%" },
        onValueChange = { onValuesChanged(values.copy(range = it / 100f)) },
    )
}

@Composable
private fun ControllerSelectDialog(
    controllers: List<InputController>,
    onDismissRequest: () -> Unit,
    onSelect: (InputController) -> Unit,
) {
    Dialog(onDismissRequest = onDismissRequest) {
        Card(
            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainer),
            modifier = Modifier
                .sizeIn(maxWidth = 560.dp, maxHeight = 560.dp)
                .fillMaxWidth(),
            shape = RoundedCornerShape(16.dp),
        ) {
            Text(
                modifier = Modifier.padding(
                    start = 16.dp, end = 16.dp, top = 16.dp, bottom = 8.dp
                ),
                text = tr("Select a controller"),
                fontSize = 24.sp,
                fontWeight = FontWeight.Bold,
            )
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(vertical = 8.dp, horizontal = 16.dp)
                    .weight(weight = 1.0f, fill = false)
                    .verticalScroll(rememberScrollState()),
            ) {
                controllers.forEach {
                    Text(text = it.name, modifier = Modifier
                        .fillMaxWidth()
                        .clickable {
                            onSelect(it)
                            onDismissRequest()
                        }
                        .padding(vertical = 16.dp, horizontal = 8.dp))
                }
            }

            HorizontalDivider()

            TextButton(
                onClick = onDismissRequest,
                modifier = Modifier
                    .padding(8.dp)
                    .align(Alignment.End),
            ) {
                Text(tr("Cancel"))
            }
        }
    }
}

@Composable
private fun InputBindingPopup(
    buttonName: String,
    mapKeyEvent: (KeyEvent) -> Unit,
    mapMotionEvent: (MotionEvent) -> Unit,
    onClear: () -> Unit,
    onDismiss: () -> Unit,
) {
    LaunchedEffect(Unit) {
        GamepadInputSource.events.collect { event ->
            when (event) {
                is GamepadInputSource.InputEvent.Key -> mapKeyEvent(event.keyEvent)
                is GamepadInputSource.InputEvent.Motion -> mapMotionEvent(event.motionEvent)
            }
        }
    }

    Popup(alignment = Alignment.Center) {
        Box(
            modifier = Modifier
                .fillMaxSize()
                .background(Color.Black.copy(alpha = 0.4f))
                .clickable(
                    indication = null,
                    interactionSource = remember { MutableInteractionSource() },
                    onClick = onDismiss
                ),
            contentAlignment = Alignment.Center,
        ) {
            Card(
                colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainer),
                modifier = Modifier
                    .sizeIn(maxWidth = 560.dp, maxHeight = 560.dp)
                    .padding(16.dp),
                shape = RoundedCornerShape(16.dp),
            ) {
                Column(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(horizontal = 16.dp, vertical = 16.dp)
                ) {
                    Text(
                        text = tr("Input binding"),
                        fontSize = 20.sp,
                        fontWeight = FontWeight.Bold,
                        modifier = Modifier.padding(bottom = 8.dp)
                    )

                    Text(
                        text = tr("Trigger an input to bind it to {0}", buttonName),
                        fontSize = 16.sp,
                        modifier = Modifier.padding(bottom = 16.dp)
                    )

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
}
