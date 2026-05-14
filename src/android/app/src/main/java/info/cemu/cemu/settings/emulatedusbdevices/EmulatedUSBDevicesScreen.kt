package info.cemu.cemu.settings.emulatedusbdevices

import androidx.compose.runtime.Composable
import info.cemu.cemu.common.ui.components.ScreenContent
import info.cemu.cemu.common.ui.components.Toggle
import info.cemu.cemu.common.ui.localization.tr
import info.cemu.cemu.nativeinterface.NativeSettings

@Composable
fun EmulatedUSBDevicesSettingsScreen(navigateBack: () -> Unit) {
    ScreenContent(
        appBarText = tr("Emulated USB Devices"),
        navigateBack = navigateBack,
    ) {
        Toggle(
            tr("Emulate Skylander Portal"),
            initialCheckedState = NativeSettings::isEmulateSkylanderPortalEnabled,
            onCheckedChanged = NativeSettings::setEmulateSkylanderPortalEnabled,
        )

        Toggle(
            tr("Emulate Infinity Base"),
            initialCheckedState = NativeSettings::isEmulateInfinityBaseEnabled,
            onCheckedChanged = NativeSettings::setEmulateInfinityBaseEnabled,
        )

        Toggle(
            tr("Emulate Dimensions Toypad"),
            initialCheckedState = NativeSettings::isEmulateDimensionsToypadEnabled,
            onCheckedChanged = NativeSettings::setEmulateDimensionsToypadEnabled,
        )
    }
}
