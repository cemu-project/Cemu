package info.cemu.cemu.titlemanager.usecases

import android.content.ContentResolver
import android.content.Context
import android.net.Uri
import androidx.documentfile.provider.DocumentFile
import info.cemu.cemu.common.android.contentresolver.DocumentEntry
import info.cemu.cemu.common.android.contentresolver.walkDocumentTree
import info.cemu.cemu.common.io.copyInputStreamToFile
import info.cemu.cemu.common.string.urlDecode
import info.cemu.cemu.nativeinterface.NativeGameTitles
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.yield
import java.io.File
import java.nio.file.Path
import java.util.LinkedList
import kotlin.coroutines.cancellation.CancellationException
import kotlin.io.path.Path
import kotlin.io.path.createDirectories
import kotlin.math.max
import kotlin.random.Random
import kotlin.random.nextUInt

enum class InstallResult{
    ERROR,
    FINISHED,
    NOT_ENOUGH_SPACE,
}

private sealed class DirEntry {
    data class File(val uri: Uri, val destinationPath: Path, val size: Long) : DirEntry()
    data class Dir(val destinationPath: Path) : DirEntry()
}

class InstallTitleUseCase(
    private val scope: CoroutineScope,
    private val mlcPath: Path
) {
    private val _inProgress = MutableStateFlow(false)
    val inProgress: StateFlow<Boolean> = _inProgress

    private val _progress = MutableStateFlow<Pair<Long, Long>?>(null)
    val progress: StateFlow<Pair<Long, Long>?> = _progress

    private var installJob: Job? = null
    private var cleanupJob: Job? = null

    fun cancel() {
        installJob?.cancel()
        installJob = null
    }

    fun install(
        context: Context,
        titleUri: Uri,
        targetLocation: String,
        callback: (InstallResult) -> Unit
    ) {
        if (_inProgress.value) return

        val installPath = Path(targetLocation)
        val installFile = installPath.toFile()
        val backupFile = installPath.getBackupFile()

        _progress.value = null
        _inProgress.value = true

        val oldInstallJob = installJob
        installJob = scope.launch(Dispatchers.IO) {
            var installStarted = false

            try {
                cleanupJob?.join()
                oldInstallJob?.join()
                val contentResolver = context.contentResolver
                val buffer = ByteArray(8192)

                val (totalSize, entries) = listFilesInSourceDirs(
                    contentResolver = contentResolver,
                    titleDir = DocumentFile.fromTreeUri(context, titleUri)!!,
                    titleUri = titleUri,
                    targetLocation = targetLocation,
                )

                if (totalSize > mlcPath.toFile().freeSpace) {
                    callback(InstallResult.NOT_ENOUGH_SPACE)
                    return@launch
                }

                backupFile.deleteRecursively()
                if (installFile.exists())
                    installFile.renameTo(backupFile)

                installStarted = true
                _progress.value = 0L to totalSize
                var bytesWritten = 0L

                for (file in entries) {
                    yield()
                    when (file) {
                        is DirEntry.Dir -> file.destinationPath.createDirectories()
                        is DirEntry.File -> contentResolver.openInputStream(
                            file.uri
                        )?.use {
                            copyInputStreamToFile(it, file.destinationPath, buffer)
                            bytesWritten += file.size
                            _progress.value = bytesWritten to totalSize
                        }
                    }
                }

                if (backupFile.exists())
                    backupFile.deleteRecursively()

                NativeGameTitles.addTitleFromPath(targetLocation)

                callback(InstallResult.FINISHED)
            } catch (e: Exception) {
                if (installStarted)
                    cleanupInstall(installFile)

                if (e !is CancellationException) callback(InstallResult.ERROR)
            } finally {
                _inProgress.value = false
            }
        }
    }

    private suspend fun listFilesInSourceDirs(
        contentResolver: ContentResolver,
        titleDir: DocumentFile,
        titleUri: Uri,
        targetLocation: String,
    ): Pair<Long, LinkedList<DirEntry>> {
        val entries = LinkedList<DirEntry>()
        var totalSize = 0L

        for (sourceDir in SOURCE_DIRS) {
            val parentUri = titleDir.findFile(sourceDir)!!.uri
            val parentUriLength = titleUri.toString().length
            val uriToTargetPath: (Uri) -> Path = {
                val relativePath = it.toString().substring(parentUriLength).urlDecode()
                Path(targetLocation, relativePath)
            }

            entries += DirEntry.Dir(Path(targetLocation, sourceDir))
            contentResolver.walkDocumentTree(
                dirUri = parentUri,
                onEntry = {
                    when (it) {
                        is DocumentEntry.Directory -> {
                            entries += DirEntry.Dir(uriToTargetPath(it.uri))
                        }

                        is DocumentEntry.File -> {
                            totalSize += it.size
                            entries += DirEntry.File(
                                it.uri,
                                uriToTargetPath(it.uri),
                                it.size
                            )
                        }
                    }
                },
            )
        }

        totalSize = max(totalSize, 1L)

        return Pair(totalSize, entries)
    }

    private fun cleanupInstall(installFile: File) {
        val oldCleanupJob = cleanupJob
        cleanupJob = scope.launch(Dispatchers.IO) {
            oldCleanupJob?.join()
            val installPath = installFile.toPath()
            var tempFile: File? = null
            if (installFile.exists()) {
                val tempName = "${installPath.fileName}-${Random.nextUInt()}"
                tempFile = installPath.resolveSibling(tempName).toFile()
                installFile.renameTo(tempFile!!)
            }
            val backupInstall = installPath.getBackupFile()
            if (backupInstall.exists())
                backupInstall.renameTo(installFile)
            tempFile?.deleteRecursively()
        }
    }

    companion object {
        private val SOURCE_DIRS = arrayOf("content", "code", "meta")
        private fun Path.getBackupFile() = resolveSibling("$fileName.backup").toFile()
    }
}
