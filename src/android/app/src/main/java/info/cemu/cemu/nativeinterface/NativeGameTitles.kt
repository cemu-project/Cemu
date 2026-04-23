package info.cemu.cemu.nativeinterface

import android.graphics.Bitmap
import androidx.annotation.Keep
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.asImageBitmap

object NativeGameTitles {
    object ConsoleRegion {
        const val JPN: Int = 0x1
        const val USA: Int = 0x2
        const val EUR: Int = 0x4
        const val AUS_DEPR: Int = 0x8
        const val CHN: Int = 0x10
        const val KOR: Int = 0x20
        const val TWN: Int = 0x40
        const val AUTO: Int = 0xFF
    }


    @JvmStatic
    external fun isLoadingSharedLibrariesForTitleEnabled(gameTitleId: Long): Boolean

    @JvmStatic
    external fun setLoadingSharedLibrariesForTitleEnabled(gameTitleId: Long, enabled: Boolean)

    object CPUMode {
        const val SINGLECOREINTERPRETER: Int = 0
        const val SINGLECORERECOMPILER: Int = 1
        const val MULTICORERECOMPILER: Int = 3
        const val AUTO: Int = 4
    }


    @JvmStatic
    external fun getCpuModeForTitle(gameTitleId: Long): Int

    @JvmStatic
    external fun setCpuModeForTitle(gameTitleId: Long, cpuMode: Int)

    val THREAD_QUANTUM_VALUES = listOf(
        20000,
        45000,
        60000,
        80000,
        100000,
    )

    @JvmStatic
    external fun getThreadQuantumForTitle(gameTitleId: Long): Int

    @JvmStatic
    external fun setThreadQuantumForTitle(gameTitleId: Long, threadQuantum: Int)

    @JvmStatic
    external fun isShaderMultiplicationAccuracyForTitleEnabled(gameTitleId: Long): Boolean

    @JvmStatic
    external fun setShaderMultiplicationAccuracyForTitleEnabled(gameTitleId: Long, enabled: Boolean)

    object DriverSettingMode {
        const val GLOBAL: Int = 0
        const val SYSTEM: Int = 1
        const val CUSTOM: Int = 2
    }

    @Keep
    data class DriverSetting(
        val mode: Int,
        val customPath: String? = null,
    )

    @JvmStatic
    external fun getDriverSettingForTitle(gameTitleId: Long): DriverSetting

    @JvmStatic
    external fun setDriverSettingForTitle(gameTitleId: Long, driverSetting: DriverSetting)

    @JvmStatic
    external fun titleHasShaderCacheFiles(gameTitleId: Long): Boolean

    @JvmStatic
    external fun removeShaderCacheFilesForTitle(gameTitleId: Long)

    @JvmStatic
    external fun setGameTitleFavorite(gameTitleId: Long, isFavorite: Boolean)

    @JvmStatic
    external fun setGameTitleLoadedCallback(gameTitleLoadedCallback: GameTitleLoadedCallback?)

    @JvmStatic
    external fun reloadGameTitles()

    @JvmStatic
    external fun getInstalledGamesTitleIds(): LongArray

    @Keep
    data class Game(
        val titleId: Long,
        val path: String,
        val name: String?,
        val version: Short,
        val dlc: Short,
        val region: Int,
        val lastPlayedYear: Short,
        val lastPlayedMonth: Short,
        val lastPlayedDay: Short,
        val minutesPlayed: Int,
        val isFavorite: Boolean,
        private val _icon: Bitmap?,
    ) : Comparable<Game> {
        override fun compareTo(other: Game): Int {
            if (titleId == other.titleId) {
                return 0
            }
            if (isFavorite && !other.isFavorite) {
                return -1
            }
            if (!isFavorite && other.isFavorite) {
                return 1
            }
            if (name == other.name) {
                return titleId.compareTo(other.titleId)
            }
            return name?.compareTo(other.name ?: "") ?: 0
        }

        private var iconBitmap: ImageBitmap? = null
        val icon: ImageBitmap?
            get() {
                if (iconBitmap == null) {
                    iconBitmap = _icon?.asImageBitmap()
                }

                return iconBitmap
            }
    }

    @Keep
    fun interface GameTitleLoadedCallback {
        fun onGameTitleLoaded(game: Game)
    }

    @Keep
    data class SaveData(
        val name: String,
        val path: String,
        val titleId: Long,
        val locationUID: Long,
        val version: Short,
        val region: Int,
    )

    @Keep
    fun interface SaveListCallback {
        fun onSaveDiscovered(saveData: SaveData)
    }

    @JvmStatic
    external fun setSaveListCallback(saveListCallback: SaveListCallback?)

    object TitleType {
        const val UNKNOWN: Int = 0xFF
        const val BASE_TITLE: Int = 0x00
        const val BASE_TITLE_DEMO: Int = 0x02
        const val BASE_TITLE_UPDATE: Int = 0x0E
        const val HOMEBREW: Int = 0x0F
        const val AOC: Int = 0x0C
        const val SYSTEM_TITLE: Int = 0x10
        const val SYSTEM_DATA: Int = 0x1B
        const val SYSTEM_OVERLAY_TITLE: Int = 0x30
    }

    object TitleDataFormat {
        const val HOST_FS: Int = 1
        const val WUD: Int = 2
        const val WIIU_ARCHIVE: Int = 3
        const val NUS: Int = 4
        const val WUHB: Int = 5
        const val INVALID_STRUCTURE: Int = 0
    }

    @Keep
    data class TitleData(
        val name: String,
        val path: String,
        val titleId: Long,
        val locationUID: Long,
        val version: Short,
        val region: Int,
        val titleType: Int,
        val titleDataFormat: Int,
    )

    @Keep
    interface TitleListCallbacks {
        fun onTitleDiscovered(titleData: TitleData)
        fun onTitleRemoved(locationUID: Long)
    }

    @JvmStatic
    external fun refreshCafeTitleList()

    @JvmStatic
    external fun setTitleListCallbacks(titleListCallbacks: TitleListCallbacks?)

    @Keep
    sealed class TitleExistsError {
        data object None : TitleExistsError()
        data class DifferentType(val oldType: Int, val toInstallType: Int) : TitleExistsError()
        data object SameVersion : TitleExistsError()
        data object NewVersion : TitleExistsError()
    }

    @Keep
    data class TitleExistsStatus(val existsError: TitleExistsError, val targetLocation: String)

    @JvmStatic
    external fun checkIfTitleExists(metaPath: String): TitleExistsStatus?

    @JvmStatic
    external fun addTitleFromPath(path: String)

    @Keep
    fun interface TitleIdToTitlesCallback {
        data class Title(val version: Short, val titleUID: Long)

        fun getTitlesByTitleId(titleId: Long): Array<Title>
    }

    @Keep
    data class CompressTitleInfo(
        val basePrintPath: String?,
        val updatePrintPath: String?,
        val aocPrintPath: String?,
    )

    @JvmStatic
    external fun queueTitleToCompress(
        titleId: Long,
        selectedUID: Long,
        titlesCallback: TitleIdToTitlesCallback,
    ): CompressTitleInfo

    @JvmStatic
    external fun getCompressedFileNameForQueuedTitle(): String?

    @Keep
    interface TitleCompressCallbacks {
        fun onFinished()
        fun onError()
    }

    @JvmStatic
    external fun compressQueuedTitle(fd: Int, compressCallbacks: TitleCompressCallbacks)

    enum class CompressResult {
        FINISHED,
        ERROR,
    }

    fun compressQueuedTitle(fd: Int, callback: (CompressResult) -> Unit) =
        compressQueuedTitle(fd, object : TitleCompressCallbacks {
            override fun onFinished() = callback(CompressResult.FINISHED)
            override fun onError() = callback(CompressResult.ERROR)
        })

    object CompressionStage {
        const val STARTING = 0
        const val CANCELLED = 1
        const val COLLECTING_FILES = 2
        const val COMPRESSING = 3
        const val FINALIZING = 4
    }

    @JvmStatic
    external fun getCurrentCompressionStage(): Int

    @JvmStatic
    external fun getCurrentCompressionTotalFileCount(): Int

    @Keep
    data class CompressionProgress(val current: Long, val total: Long)

    @JvmStatic
    external fun getCurrentProgressForCompression(): CompressionProgress?

    @JvmStatic
    external fun cancelTitleCompression()
}
