@file:Suppress("unused")

package info.cemu.cemu.nativeinterface

import android.content.ContentResolver
import android.net.Uri
import android.provider.DocumentsContract
import android.util.Log
import androidx.annotation.Keep
import androidx.core.net.toUri

private const val PATH_SEPARATOR_ENCODED = "%2F"
private const val PATH_SEPARATOR_DECODED = "/"
private const val COLON_ENCODED = "%3A"
private const val MODE = "r"

fun Uri.toNativePath(): String {
    val uriPath = toString()
    val delimiterPos = uriPath.lastIndexOf(COLON_ENCODED)
    if (delimiterPos == -1) {
        return uriPath
    }
    return uriPath.take(delimiterPos) + uriPath.substring(delimiterPos).replace(
        PATH_SEPARATOR_ENCODED, PATH_SEPARATOR_DECODED
    )
}

fun String.fromNativePath(): Uri {
    val delimiterPos = lastIndexOf(COLON_ENCODED)
    if (delimiterPos == -1) {
        return toUri()
    }

    return (substring(0, delimiterPos) + substring(delimiterPos).replace(
        PATH_SEPARATOR_DECODED, PATH_SEPARATOR_ENCODED
    )).toUri()
}

object NativeFiles {
    private lateinit var contentResolver: ContentResolver

    fun initialize(contentResolver: ContentResolver) {
        this.contentResolver = contentResolver
    }

    @Keep
    @JvmStatic
    fun openContentUri(uri: String): Int {
        try {
            val parcelFileDescriptor =
                contentResolver.openFileDescriptor(
                    uri.fromNativePath(), MODE
                )
            if (parcelFileDescriptor != null) {
                val fd = parcelFileDescriptor.detachFd()
                parcelFileDescriptor.close()
                return fd
            }
        } catch (e: Exception) {
            Log.d("NativeFiles", "Cannot open content uri, error: ${e.message}")
        }
        return -1
    }

    @Keep
    @JvmStatic
    fun listFiles(uri: String): Array<String?> {
        val files = ArrayList<String>()
        val directoryUri = uri.fromNativePath()
        val childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(
            directoryUri,
            DocumentsContract.getDocumentId(directoryUri)
        )
        try {
            contentResolver.query(
                childrenUri,
                arrayOf(DocumentsContract.Document.COLUMN_DOCUMENT_ID),
                null,
                null,
                null
            ).use { cursor ->
                while (cursor != null && cursor.moveToNext()) {
                    val documentId = cursor.getString(0)
                    val documentUri =
                        DocumentsContract.buildDocumentUriUsingTree(directoryUri, documentId)
                    files.add(documentUri.toNativePath())
                }
            }
        } catch (e: Exception) {
            Log.d("NativeFiles", "Cannot list files: ${e.message}")
        }
        var filesArray = arrayOfNulls<String>(files.size)
        filesArray = files.toArray(filesArray)
        return filesArray
    }

    @Keep
    @JvmStatic
    fun isDirectory(uri: String): Boolean {
        val mimeType = contentResolver.getType(
            uri.fromNativePath()
        )
        return DocumentsContract.Document.MIME_TYPE_DIR == mimeType
    }

    @Keep
    @JvmStatic
    fun isFile(uri: String): Boolean {
        return !isDirectory(uri)
    }

    @Keep
    @JvmStatic
    fun exists(uri: String): Boolean {
        try {
            contentResolver.query(uri.fromNativePath(), null, null, null, null).use { cursor ->
                return cursor != null && cursor.moveToFirst()
            }
        } catch (e: Exception) {
            Log.d("NativeFiles", "Failed checking if file exists: ${e.message}")
            return false
        }
    }
}
