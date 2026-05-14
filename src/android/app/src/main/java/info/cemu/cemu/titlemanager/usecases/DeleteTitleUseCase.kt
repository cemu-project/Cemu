package info.cemu.cemu.titlemanager.usecases

import android.content.ContentResolver
import android.content.Context
import android.provider.DocumentsContract
import info.cemu.cemu.common.string.isContentUri
import info.cemu.cemu.nativeinterface.fromNativePath
import info.cemu.cemu.titlemanager.TitleEntry
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlin.io.path.Path

enum class DeleteResult
{
    FINISHED,
    ERROR,
}

class DeleteTitleUseCase(
    private val scope: CoroutineScope
) {
    fun delete(
        context: Context,
        titleEntry: TitleEntry,
        callback: (DeleteResult) -> Unit,
    ) {
        scope.launch(Dispatchers.IO) {
            try {
                if (delete(context.contentResolver, titleEntry.path))
                    callback(DeleteResult.FINISHED)
                else
                    callback(DeleteResult.ERROR)
            } catch (_: Exception) {
                callback(DeleteResult.ERROR)
            }
        }
    }

    private fun delete(contentResolver: ContentResolver, path: String): Boolean {
        if (path.isContentUri()) {
            return DocumentsContract.deleteDocument(
                contentResolver,
                path.fromNativePath()
            )
        }

        return Path(path).toFile().deleteRecursively()
    }
}
