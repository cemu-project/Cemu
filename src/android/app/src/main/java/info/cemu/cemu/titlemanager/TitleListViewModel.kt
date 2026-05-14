package info.cemu.cemu.titlemanager

import android.content.Context
import android.net.Uri
import android.os.SystemClock
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import info.cemu.cemu.common.collections.toggleInSet
import info.cemu.cemu.nativeinterface.NativeActiveSettings
import info.cemu.cemu.nativeinterface.NativeGameTitles
import info.cemu.cemu.nativeinterface.NativeGameTitles.TitleIdToTitlesCallback.Title
import info.cemu.cemu.titlemanager.usecases.CompressResult
import info.cemu.cemu.titlemanager.usecases.CompressTitleUseCase
import info.cemu.cemu.titlemanager.usecases.DeleteResult
import info.cemu.cemu.titlemanager.usecases.DeleteTitleUseCase
import info.cemu.cemu.titlemanager.usecases.InstallResult
import info.cemu.cemu.titlemanager.usecases.InstallTitleUseCase
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.stateIn
import kotlin.io.path.Path
import kotlin.io.path.relativeToOrNull

enum class EntryPath {
    MLC,
    GamePaths,
}

data class TitleListFilter(
    val query: String,
    val types: Set<EntryType>,
    val formats: Set<EntryFormat>,
    val paths: Set<EntryPath>,
)

class TitleListViewModel : ViewModel() {
    private val mlcPath = Path(NativeActiveSettings.getMLCPath())
    private val installUseCase = InstallTitleUseCase(viewModelScope, mlcPath)
    private val deleteUseCase = DeleteTitleUseCase(viewModelScope)
    private val compressUseCase = CompressTitleUseCase(viewModelScope)

    private fun isPathInMLC(path: String): Boolean =
        Path(path).relativeToOrNull(mlcPath)?.let { it.startsWith("sys") || it.startsWith("usr") }
            ?: false

    private val _filter = MutableStateFlow(
        TitleListFilter(
            query = "",
            types = EntryType.entries.toSet(),
            formats = EntryFormat.entries.toSet(),
            paths = EntryPath.entries.toSet(),
        )
    )
    val filter = _filter.asStateFlow()

    private fun updateFilter(transform: (TitleListFilter) -> TitleListFilter) {
        _filter.value = transform(_filter.value)
    }

    fun setFilterQuery(query: String) {
        updateFilter { it.copy(query = query) }
    }

    fun toggleType(type: EntryType) {
        updateFilter { it.copy(types = it.types.toggleInSet(type)) }
    }

    fun toggleFormat(format: EntryFormat) {
        updateFilter { it.copy(formats = it.formats.toggleInSet(format)) }
    }

    fun togglePath(path: EntryPath) {
        updateFilter { it.copy(paths = it.paths.toggleInSet(path)) }
    }

    private val _titleEntries = MutableStateFlow<List<TitleEntry>>(emptyList())
    val titleEntries = filter.combine(_titleEntries) { filter, entries ->
        entries.filter { entry ->
            val isTypeMatching = entry.type in filter.types
            val isFormatMatching = entry.format in filter.formats
            val isPathMatching = when {
                entry.isInMLC -> EntryPath.MLC in filter.paths
                else -> EntryPath.GamePaths in filter.paths
            }
            val isQueryMatching =
                filter.query.isBlank() || entry.name.contains(filter.query, ignoreCase = true)
            isTypeMatching && isFormatMatching && isPathMatching && isQueryMatching
        }.sortedBy { it.name }
    }.stateIn(
        viewModelScope,
        SharingStarted.WhileSubscribed(5000),
        emptyList()
    )

    private val titleListCallbacks = object : NativeGameTitles.TitleListCallbacks {
        override fun onTitleDiscovered(titleData: NativeGameTitles.TitleData) {
            addTitle(titleData.toTitleEntry(isPathInMLC(titleData.path)))
        }

        override fun onTitleRemoved(locationUID: Long) {
            _titleEntries.value =
                _titleEntries.value.toMutableList()
                    .apply {
                        val index = indexOfFirst { it.locationUID == locationUID }
                        if (index >= 0) removeAt(index)
                    }
        }
    }

    private val saveListCallback = NativeGameTitles.SaveListCallback { saveData ->
        addTitle(saveData.toTitleEntry(isPathInMLC(saveData.path)))
    }

    private var lastRefreshTime = 0L
    fun refresh() {
        if (SystemClock.elapsedRealtime() - lastRefreshTime >= REFRESH_DEBOUNCE_TIME_MILLISECONDS) {
            lastRefreshTime = SystemClock.elapsedRealtime()
            NativeGameTitles.refreshCafeTitleList()
        }
    }

    init {
        NativeGameTitles.setTitleListCallbacks(titleListCallbacks)
        NativeGameTitles.setSaveListCallback(saveListCallback)
    }

    override fun onCleared() {
        super.onCleared()
        NativeGameTitles.setTitleListCallbacks(null)
        NativeGameTitles.setSaveListCallback(null)
    }

    private fun addTitle(titleEntry: TitleEntry) {
        if (_titleEntries.value.any { it.locationUID == titleEntry.locationUID }) return
        _titleEntries.value += titleEntry
    }

    private val _titleToBeDeleted = MutableStateFlow<TitleEntry?>(null)
    val titleToBeDeleted = _titleToBeDeleted.asStateFlow()
    fun deleteTitleEntry(
        titleEntry: TitleEntry,
        context: Context,
        callback: (DeleteResult) -> Unit,
    ) {
        if (_titleToBeDeleted.value != null)
            return

        if (!_titleEntries.value.any { it.locationUID == titleEntry.locationUID })
            return

        _titleToBeDeleted.value = titleEntry

        deleteUseCase.delete(
            context = context,
            titleEntry = titleEntry,
            callback = { result ->
                if (result == DeleteResult.FINISHED) {
                    _titleEntries.value = _titleEntries.value.filterNot {
                        it.locationUID == titleEntry.locationUID && it.path == titleEntry.path
                    }
                }

                _titleToBeDeleted.value = null

                callback(result)
            }
        )
    }

    private val _queuedTitleToInstall =
        MutableStateFlow<Pair<Uri, NativeGameTitles.TitleExistsStatus>?>(null)
    val queuedTitleToInstallError =
        _queuedTitleToInstall.map { it?.second?.existsError }.stateIn(
            viewModelScope,
            SharingStarted.WhileSubscribed(5000),
            null
        )

    fun queueTitleToInstall(titlePath: Uri, onInvalidTitle: () -> Unit) {
        if (_queuedTitleToInstall.value != null)
            return

        val existsStatus = NativeGameTitles.checkIfTitleExists(titlePath.toString())
        if (existsStatus == null) {
            onInvalidTitle()
            return
        }

        _queuedTitleToInstall.value = Pair(titlePath, existsStatus)
    }

    fun removedQueuedTitleToInstall() {
        _queuedTitleToInstall.value = null
    }


    val titleInstallInProgress = installUseCase.inProgress
    val titleInstallProgress = installUseCase.progress

    fun installQueuedTitle(context: Context, callback: (InstallResult) -> Unit) {
        val (titleUri, titleExistsStatus) = _queuedTitleToInstall.value ?: return
        _queuedTitleToInstall.value = null

        installUseCase.install(
            context = context,
            titleUri = titleUri,
            targetLocation = titleExistsStatus.targetLocation,
            callback = callback,
        )
    }

    fun cancelInstall() = installUseCase.cancel()

    private val _queuedTitleToCompress =
        MutableStateFlow<NativeGameTitles.CompressTitleInfo?>(null)
    val queuedTitleToCompress = _queuedTitleToCompress.asStateFlow()
    fun queueTitleForCompression(titleEntry: TitleEntry) {
        require(
            value = titleEntry.type != EntryType.Save && titleEntry.format != EntryFormat.WUA,
            lazyMessage = { "Invalid title queued for compression. ${titleEntry.name} (${titleEntry.type.name}) (${titleEntry.format.name})" }
        )

        _queuedTitleToCompress.value = NativeGameTitles.queueTitleToCompress(
            titleId = titleEntry.titleId,
            selectedUID = titleEntry.locationUID,
            titlesCallback = { titleId ->
                titleEntries.value.filter { it.titleId == titleId }
                    .map { Title(it.version, it.locationUID) }
                    .toTypedArray()
            })
    }

    fun removeQueuedTitleForCompression() {
        _queuedTitleToCompress.value = null
    }

    val compressStage = compressUseCase.stage
    val compressProgress = compressUseCase.progress
    val compressionTotalFileCount = compressUseCase.totalFileCount


    fun compressQueuedTitle(
        context: Context,
        uri: Uri,
        onResult: (CompressResult) -> Unit
    ) {
        _queuedTitleToCompress.value = null
        compressUseCase.compress(context, uri, onResult)
    }

    fun cancelCompression() = compressUseCase.cancel()

    fun getCompressedFileNameForQueuedTitle() =
        NativeGameTitles.getCompressedFileNameForQueuedTitle()

    companion object {
        private const val REFRESH_DEBOUNCE_TIME_MILLISECONDS = 1500L
    }
}

private fun NativeGameTitles.SaveData.toTitleEntry(isInMlc: Boolean) = TitleEntry(
    titleId = this.titleId,
    name = this.name,
    path = this.path,
    isInMLC = isInMlc,
    locationUID = this.locationUID,
    version = this.version,
    region = this.region,
    type = EntryType.Save,
    format = EntryFormat.SaveFolder,
)

private fun NativeGameTitles.TitleData.toTitleEntry(isInMlc: Boolean) = TitleEntry(
    titleId = this.titleId,
    name = this.name,
    path = this.path,
    isInMLC = isInMlc,
    locationUID = this.locationUID,
    version = this.version,
    region = this.region,
    type = nativeTitleTypeToEnum(this.titleType),
    format = nativeTitleFormatToEnum(this.titleDataFormat),
)
