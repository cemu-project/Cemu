package info.cemu.cemu.titlemanager

import info.cemu.cemu.nativeinterface.NativeGameTitles

data class TitleEntry(
    val titleId: Long,
    val name: String,
    val path: String,
    val isInMLC: Boolean,
    val locationUID: Long,
    val version: Short,
    val region: Int,
    val type: EntryType,
    val format: EntryFormat,
) {
    init {
        if (type == EntryType.Save || format == EntryFormat.SaveFolder) {
            require(type == EntryType.Save && format == EntryFormat.SaveFolder)
        }
    }
}

enum class EntryType {
    Base,
    Update,
    Dlc,
    Save,
    System,
}

fun nativeTitleTypeToEnum(titleType: Int) = when (titleType) {
    NativeGameTitles.TitleType.BASE_TITLE_UPDATE -> EntryType.Update
    NativeGameTitles.TitleType.AOC -> EntryType.Dlc
    NativeGameTitles.TitleType.SYSTEM_OVERLAY_TITLE, NativeGameTitles.TitleType.SYSTEM_DATA, NativeGameTitles.TitleType.SYSTEM_TITLE -> EntryType.System
    else -> EntryType.Base
}

enum class EntryFormat {
    SaveFolder,
    Folder,
    WUD,
    NUS,
    WUA,
    WUHB,
}

fun nativeTitleFormatToEnum(titleFormat: Int) = when (titleFormat) {
    NativeGameTitles.TitleDataFormat.WUD -> EntryFormat.WUD
    NativeGameTitles.TitleDataFormat.WIIU_ARCHIVE -> EntryFormat.WUA
    NativeGameTitles.TitleDataFormat.NUS -> EntryFormat.NUS
    NativeGameTitles.TitleDataFormat.WUHB -> EntryFormat.WUHB
    else -> EntryFormat.Folder
}
