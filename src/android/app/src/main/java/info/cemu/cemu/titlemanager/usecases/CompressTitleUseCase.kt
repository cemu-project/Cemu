package info.cemu.cemu.titlemanager.usecases

import android.content.Context
import android.net.Uri
import info.cemu.cemu.nativeinterface.NativeGameTitles
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.cancelAndJoin
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch

private typealias NativeCompressResult = NativeGameTitles.CompressResult

enum class CompressResult {
    FINISHED,
    ERROR,
}

enum class CompressionStage {
    STARTING,
    COLLECTING_FILES,
    COMPRESSING,
    FINALIZING,
}

data class CompressionProgress(
    val current: Long,
    val total: Long
)

class CompressTitleUseCase(private val scope: CoroutineScope) {
    private val _stage = MutableStateFlow<CompressionStage?>(null)
    val stage = _stage.asStateFlow()

    private val _progress = MutableStateFlow(CompressionProgress(0, 0))
    val progress = _progress.asStateFlow()

    private val _totalFileCount = MutableStateFlow(0)
    val totalFileCount = _totalFileCount.asStateFlow()

    private var compressJob: Job? = null
    private var progressJob: Job? = null


    fun cancel() {
        scope.launch(Dispatchers.IO) {
            NativeGameTitles.cancelTitleCompression()
        }
    }

    fun compress(
        context: Context,
        uri: Uri,
        callback: (CompressResult) -> Unit
    ) {
        if (_stage.value != null) return

        val fd = context.contentResolver.openFileDescriptor(uri, "rw")

        if (fd == null) {
            callback(CompressResult.ERROR)
            return
        }

        compressJob = scope.launch {
            _stage.value = CompressionStage.STARTING

            progressJob?.cancelAndJoin()

            try {
                progressJob = launch {
                    while (isActive) {
                        delay(500)
                        val currentProgress = NativeGameTitles.getCurrentProgressForCompression()

                        if (currentProgress == null) {
                            _stage.value = null
                            return@launch
                        }

                        val (current, total) = currentProgress
                        _progress.value = CompressionProgress(current, total)

                        _totalFileCount.value =
                            NativeGameTitles.getCurrentCompressionTotalFileCount()

                        val currentStage = NativeGameTitles.getCurrentCompressionStage()

                        if (currentStage == NativeGameTitles.CompressionStage.CANCELLED) {
                            _stage.value = null
                            return@launch
                        }

                        _stage.value = when (NativeGameTitles.getCurrentCompressionStage()) {
                            NativeGameTitles.CompressionStage.STARTING -> CompressionStage.STARTING
                            NativeGameTitles.CompressionStage.COLLECTING_FILES -> CompressionStage.COLLECTING_FILES
                            NativeGameTitles.CompressionStage.COMPRESSING -> CompressionStage.COMPRESSING
                            NativeGameTitles.CompressionStage.FINALIZING -> CompressionStage.FINALIZING
                            else -> CompressionStage.STARTING
                        }
                    }
                }

                NativeGameTitles.compressQueuedTitle(
                    fd = fd.detachFd(),
                    callback = { result ->
                        progressJob?.cancel()

                        when (result) {
                            NativeCompressResult.FINISHED -> callback(CompressResult.FINISHED)
                            NativeCompressResult.ERROR -> callback(CompressResult.ERROR)
                        }

                        _stage.value = null
                    }
                )
            } catch (_: Exception) {
                callback(CompressResult.ERROR)
                _stage.value = null
            }
        }
    }
}
