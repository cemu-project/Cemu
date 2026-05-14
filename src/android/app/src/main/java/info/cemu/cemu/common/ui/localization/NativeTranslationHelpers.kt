package info.cemu.cemu.common.ui.localization

import info.cemu.cemu.nativeinterface.NativeGameTitles.ConsoleRegion
import info.cemu.cemu.nativeinterface.NativeInput.EmulatedControllerType

fun regionToString(region: Int): String = when (region) {
    ConsoleRegion.JPN -> tr("Japan")
    ConsoleRegion.USA -> tr("USA")
    ConsoleRegion.EUR -> tr("Europe")
    ConsoleRegion.AUS_DEPR -> tr("Australia")
    ConsoleRegion.CHN -> tr("China")
    ConsoleRegion.KOR -> tr("Korea")
    ConsoleRegion.TWN -> tr("Taiwan")
    ConsoleRegion.AUTO -> tr("Auto")
    else -> tr("Many")
}

fun controllerTypeToString(type: Int) = when (type) {
    EmulatedControllerType.DISABLED -> tr("Disabled")
    EmulatedControllerType.VPAD -> tr("Wii U GamePad")
    EmulatedControllerType.PRO -> tr("Wii U Pro Controller")
    EmulatedControllerType.WIIMOTE -> tr("Wiimote")
    EmulatedControllerType.CLASSIC -> tr("Wii U Classic Controller")
    else -> throw IllegalArgumentException("Invalid controller type: $type")
}
