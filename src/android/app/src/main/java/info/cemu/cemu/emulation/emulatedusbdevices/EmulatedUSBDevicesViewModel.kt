package info.cemu.cemu.emulation.emulatedusbdevices

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import androidx.lifecycle.viewmodel.compose.viewModel
import info.cemu.cemu.common.coroutines.RefreshableStateFlow
import info.cemu.cemu.nativeinterface.NativeEmulatedUSBDevices
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.asSharedFlow
import kotlinx.coroutines.launch

sealed class UsbDeviceEvent {
    object CreateFailed : UsbDeviceEvent()
    object LoadFailed : UsbDeviceEvent()
}

class EmulatedUSBDevicesViewModel : ViewModel() {
    val skylanderFigures = NativeEmulatedUSBDevices.getSkylanderFigures()
    val dimensionsMiniFigures = NativeEmulatedUSBDevices.getDimensionsMiniFigures()
    val infinityFigures = NativeEmulatedUSBDevices.getInfinityFigures()

    private val _events = MutableSharedFlow<UsbDeviceEvent>()
    val events = _events.asSharedFlow()

    private fun emitEvent(usbDeviceEvent: UsbDeviceEvent) {
        viewModelScope.launch { _events.emit(usbDeviceEvent) }
    }

    private fun getFigureSlots(maxSlots: Int, slotGetter: (Int) -> String?) =
        Array(maxSlots) { slotGetter(it) }

    val skylanderSlots = RefreshableStateFlow {
        getFigureSlots(
            NativeEmulatedUSBDevices.MAX_SKYLANDERS,
            NativeEmulatedUSBDevices::getSkylandersFigureSlot
        )
    }

    val infinitySlots = RefreshableStateFlow {
        getFigureSlots(
            NativeEmulatedUSBDevices.MAX_INFINITY_SLOTS,
            NativeEmulatedUSBDevices::getInfinityFigureSlot
        )
    }

    val dimensionsSlots = RefreshableStateFlow {
        getFigureSlots(
            NativeEmulatedUSBDevices.MAX_DIMENSIONS_SLOTS,
            NativeEmulatedUSBDevices::getDimensionsFigureSlot
        )
    }

    fun clearDimensionsFigure(pad: Int, index: Int) {
        NativeEmulatedUSBDevices.clearDimensionsFigure(pad, index)
        dimensionsSlots.refresh()
    }

    fun clearInfinityFigure(slot: Int) {
        NativeEmulatedUSBDevices.clearInfinityFigure(slot)
        infinitySlots.refresh()
    }

    fun clearSkylandersFigure(slot: Int) {
        NativeEmulatedUSBDevices.clearSkylandersFigure(slot)
        skylanderSlots.refresh()
    }

    val installedSkylanderFigures =
        RefreshableStateFlow(NativeEmulatedUSBDevices::getInstalledSkylanderFigures)

    val installedDimensionMiniFigures =
        RefreshableStateFlow(NativeEmulatedUSBDevices::getInstalledDimensionsMiniFigures)

    val installedInfinityFigures =
        RefreshableStateFlow(NativeEmulatedUSBDevices::getInstalledInfinityFigures)

    private fun deleteFigure(
        installedFigure: NativeEmulatedUSBDevices.InstalledFigure, afterDelete: () -> Unit
    ) {
        NativeEmulatedUSBDevices.deleteFigure(installedFigure)
        afterDelete()
    }

    fun deleteInstalledSkylanderFigure(installedFigure: NativeEmulatedUSBDevices.InstalledFigure) =
        deleteFigure(installedFigure, installedSkylanderFigures::refresh)

    fun deleteInstalledDimensionsFigure(installedFigure: NativeEmulatedUSBDevices.InstalledFigure) =
        deleteFigure(installedFigure, installedDimensionMiniFigures::refresh)

    fun deleteInstalledInfinityFigure(installedFigure: NativeEmulatedUSBDevices.InstalledFigure) =
        deleteFigure(installedFigure, installedInfinityFigures::refresh)

    fun createSkylanderFigure(id: Int, variant: Int) {
        if (!NativeEmulatedUSBDevices.createSkylanderFigure(id, variant)) {
            emitEvent(UsbDeviceEvent.CreateFailed)
            return
        }
        installedSkylanderFigures.refresh()
    }

    fun createDimensionsMiniFigure(number: Long) {
        if (!NativeEmulatedUSBDevices.createDimensionsFigure(number)) {
            emitEvent(UsbDeviceEvent.CreateFailed)
            return
        }
        installedDimensionMiniFigures.refresh()
    }

    fun createInfinityFigure(number: Long) {
        if (!NativeEmulatedUSBDevices.createInfinityFigure(number)) {
            emitEvent(UsbDeviceEvent.CreateFailed)
            return
        }
        installedInfinityFigures.refresh()
    }

    fun loadSkylanderFigure(figure: NativeEmulatedUSBDevices.InstalledFigure, slot: Int) {
        if (!NativeEmulatedUSBDevices.loadSkylandersFigure(figure.path, slot)) {
            emitEvent(UsbDeviceEvent.LoadFailed)
            return
        }
        skylanderSlots.refresh()
    }

    fun loadDimensionsFigure(
        figure: NativeEmulatedUSBDevices.InstalledFigure, pad: Int, index: Int
    ) {
        if (!NativeEmulatedUSBDevices.loadDimensionsFigure(figure.path, pad, index)) {
            emitEvent(UsbDeviceEvent.LoadFailed)
            return
        }
        dimensionsSlots.refresh()
    }

    fun loadInfinityFigure(figure: NativeEmulatedUSBDevices.InstalledFigure, slot: Int) {
        if (!NativeEmulatedUSBDevices.loadInfinityFigure(figure.path, slot)) {
            emitEvent(UsbDeviceEvent.LoadFailed)
            return
        }
        infinitySlots.refresh()
    }

    fun tempRemoveDimensionsFigure(index: Int) =
        NativeEmulatedUSBDevices.tempRemoveDimensionsFigure(index)

    fun cancelRemoveDimensionsFigure(index: Int) =
        NativeEmulatedUSBDevices.cancelRemoveDimensionsFigure(index)

    fun moveDimensionsFigure(pad: Int, index: Int, oldPad: Int, oldIndex: Int) {
        NativeEmulatedUSBDevices.moveDimensionsFigure(pad, index, oldPad, oldIndex)
        dimensionsSlots.refresh()
    }
}