package info.cemu.cemu.settings.audio

import androidx.compose.runtime.Composable
import info.cemu.cemu.common.ui.components.ScreenContent
import info.cemu.cemu.common.ui.components.SingleSelection
import info.cemu.cemu.common.ui.components.Slider
import info.cemu.cemu.common.ui.components.Toggle
import info.cemu.cemu.common.ui.localization.tr
import info.cemu.cemu.nativeinterface.NativeSettings

private const val AUDIO_LATENCY_STEPS = 22
private const val AUDIO_VOLUME_STEPS = 19

@Composable
fun AudioSettingsScreen(navigateBack: () -> Unit) {
    ScreenContent(
        appBarText = tr("Audio settings"),
        navigateBack = navigateBack,
    ) {
        Slider(
            label = tr("Latency"),
            initialValue = NativeSettings::getAudioLatency,
            valueFrom = 0,
            steps = AUDIO_LATENCY_STEPS,
            valueTo = NativeSettings.AUDIO_LATENCY_MS_MAX,
            onValueChange = NativeSettings::setAudioLatency,
            labelFormatter = { "${it}ms" }
        )
        Toggle(
            label = tr("TV"),
            description = tr("Enable audio output for the Wii U TV"),
            initialCheckedState = { NativeSettings.getAudioDeviceEnabled(true) },
            onCheckedChanged = { NativeSettings.setAudioDeviceEnabled(it, true) }
        )
        SingleSelection(
            label = tr("TV channels"),
            initialChoice = { NativeSettings.getAudioDeviceChannels(true) },
            onChoiceChanged = { NativeSettings.setAudioDeviceChannels(it, true) },
            choiceToString = { channelsToString(it) },
            choices = listOf(
                NativeSettings.AudioChannels.MONO,
                NativeSettings.AudioChannels.STEREO,
                NativeSettings.AudioChannels.SURROUND,
            ),
        )
        Slider(
            label = tr("TV volume"),
            initialValue = { NativeSettings.getAudioDeviceVolume(true) },
            valueFrom = NativeSettings.AUDIO_MIN_VOLUME,
            steps = AUDIO_VOLUME_STEPS,
            valueTo = NativeSettings.AUDIO_MAX_VOLUME,
            onValueChange = { NativeSettings.setAudioDeviceVolume(it, true) },
            labelFormatter = { "$it%" }
        )
        Toggle(
            label = tr("Gamepad"),
            description = tr("Enable audio output for the Wii U Gamepad"),
            initialCheckedState = { NativeSettings.getAudioDeviceEnabled(false) },
            onCheckedChanged = { NativeSettings.setAudioDeviceEnabled(it, false) }
        )
        SingleSelection(
            label = tr("Gamepad channels"),
            initialChoice = { NativeSettings.getAudioDeviceChannels(false) },
            onChoiceChanged = { NativeSettings.setAudioDeviceChannels(it, false) },
            choiceToString = { channelsToString(it) },
            choices = listOf(
                NativeSettings.AudioChannels.STEREO,
            ),
        )
        Slider(
            label = tr("Gamepad volume"),
            initialValue = { NativeSettings.getAudioDeviceVolume(false) },
            valueFrom = NativeSettings.AUDIO_MIN_VOLUME,
            steps = AUDIO_VOLUME_STEPS,
            valueTo = NativeSettings.AUDIO_MAX_VOLUME,
            onValueChange = { NativeSettings.setAudioDeviceVolume(it, false) },
            labelFormatter = { "$it%" }
        )
    }
}

fun channelsToString(channels: Int) = when (channels) {
    NativeSettings.AudioChannels.MONO -> tr("Mono")
    NativeSettings.AudioChannels.STEREO -> tr("Stereo")
    NativeSettings.AudioChannels.SURROUND -> tr("Surround")
    else -> throw IllegalArgumentException("Invalid channels type: $channels")
}
