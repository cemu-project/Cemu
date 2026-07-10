package info.cemu.cemu.nativeinterface

import androidx.annotation.Keep

object NativeEmulatedUSBDevices {
    @Keep
    data class InstalledFigure(
        val name: String,
        val path: String,
    )

    const val MAX_INFINITY_SLOTS = 9

    @Keep
    data class InfinityFigure(
        val number: UInt,
        val series: UByte,
        val name: String,
    )

    @JvmStatic
    external fun getInfinityFigures(): Array<InfinityFigure>

    @JvmStatic
    external fun getInstalledInfinityFigures(): Array<InstalledFigure>

    const val MAX_DIMENSIONS_SLOTS = 7

    @Keep
    data class DimensionsMiniFigure(
        val number: UInt,
        val name: String,
    )

    @JvmStatic
    external fun getDimensionsMiniFigures(): Array<DimensionsMiniFigure>

    @JvmStatic
    external fun getInstalledDimensionsMiniFigures(): Array<InstalledFigure>

    const val SKYLANDER_MAX_ID = 0xFFFF
    const val SKYLANDER_MAX_VARIANT = 0xFFFF
    const val MAX_SKYLANDERS = 16

    @Keep
    data class SkylanderFigure(
        val id: UShort,
        val variant: UShort,
        val name: String,
    )

    @JvmStatic
    external fun getSkylanderFigures(): Array<SkylanderFigure>

    @JvmStatic
    external fun getInstalledSkylanderFigures(): Array<InstalledFigure>

    @JvmStatic
    external fun deleteFigure(installedFigure: InstalledFigure): Boolean

    @JvmStatic
    external fun createSkylanderFigure(id: Int, variant: Int): Boolean

    @JvmStatic
    external fun createInfinityFigure(number: Long): Boolean

    @JvmStatic
    external fun createDimensionsFigure(number: Long): Boolean

    @JvmStatic
    external fun loadDimensionsFigure(path: String, pad: Int, index: Int): Boolean

    @JvmStatic
    external fun loadInfinityFigure(path: String, slot: Int): Boolean

    @JvmStatic
    external fun loadSkylandersFigure(path: String, slot: Int): Boolean

    @JvmStatic
    external fun getSkylandersFigureSlot(slot: Int): String?

    @JvmStatic
    external fun getInfinityFigureSlot(slot: Int): String?

    @JvmStatic
    external fun getDimensionsFigureSlot(slot: Int): String?

    @JvmStatic
    external fun clearDimensionsFigure(pad: Int, index: Int)

    @JvmStatic
    external fun clearInfinityFigure(slot: Int)

    @JvmStatic
    external fun clearSkylandersFigure(slot: Int)

    @JvmStatic
    external fun tempRemoveDimensionsFigure(index: Int)

    @JvmStatic
    external fun cancelRemoveDimensionsFigure(index: Int)

    @JvmStatic
    external fun moveDimensionsFigure(
        newPad: Int,
        newIndex: Int,
        oldPad: Int,
        oldIndex: Int,
    )
}